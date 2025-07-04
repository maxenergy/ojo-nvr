package it.danieleverducci.ojo.ui;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.Window;
import android.view.WindowManager;
import android.widget.LinearLayout;

import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;
import androidx.fragment.app.Fragment;

// ExoPlayer imports for RTSP streaming
import com.google.android.exoplayer2.ExoPlayer;
import com.google.android.exoplayer2.MediaItem;
import com.google.android.exoplayer2.PlaybackException;
import com.google.android.exoplayer2.Player;
import com.google.android.exoplayer2.source.MediaSource;
import com.google.android.exoplayer2.source.ProgressiveMediaSource;
import com.google.android.exoplayer2.source.rtsp.RtspMediaSource;
import com.google.android.exoplayer2.ui.PlayerView;
import com.google.android.exoplayer2.upstream.DefaultDataSource;
import com.google.android.exoplayer2.DefaultLoadControl;
import com.google.android.exoplayer2.DefaultRenderersFactory;
import com.google.android.exoplayer2.LoadControl;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import it.danieleverducci.ojo.R;
import it.danieleverducci.ojo.Settings;
import it.danieleverducci.ojo.databinding.FragmentSurveillanceBinding;
import it.danieleverducci.ojo.entities.Camera;
import it.danieleverducci.ojo.utils.DpiUtils;
import it.danieleverducci.ojo.utils.PerformanceMonitor;

/**
 * Some streams to test:
 * rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov
 * rtsp://demo:demo@ipvmdemo.dyndns.org:5541/onvif-media/media.amp?profile=profile_1_h264&sessiontimeout=60&streamtype=unicast
 */
public class SurveillanceFragment extends Fragment {

    final static private String TAG = "SurveillanceFragment";

    // Player states for better error handling
    protected enum PlayerState {
        IDLE, PREPARING, READY, BUFFERING, ERROR, ENDED
    }

    // ExoPlayer configuration constants
    private static final long RTSP_TIMEOUT_MS = 10000; // 10 seconds timeout
    private static final boolean ENABLE_AUDIO = true;  // Enable audio for RTSP streams
    private static final int MAX_RETRY_ATTEMPTS = 3;   // Maximum retry attempts on error
    private static final long RETRY_DELAY_MS = 5000;   // Delay between retry attempts

    // Performance optimization constants
    private static final int MAX_CONCURRENT_STREAMS = 4; // Limit concurrent streams for performance
    private static final long BUFFER_SIZE_MS = 3000;     // 3 seconds buffer
    private static final long MIN_BUFFER_MS = 1000;      // 1 second minimum buffer
    private static final boolean ENABLE_HARDWARE_ACCELERATION = true; // Use hardware decoding when available

    // Test streams for debugging (mix of HTTP and RTSP)
    private static final String[] TEST_STREAMS = {
        "https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4",
        "https://sample-videos.com/zip/10/mp4/SampleVideo_1280x720_1mb.mp4"
    };

    private FragmentSurveillanceBinding binding;
    private List<CameraView> cameraViews = new ArrayList<>();
    private boolean fullscreenCameraView = false;
    private LinearLayout.LayoutParams cameraViewLayoutParams;
    private LinearLayout.LayoutParams rowLayoutParams;
    private LinearLayout.LayoutParams hiddenLayoutParams;
    private PerformanceMonitor performanceMonitor;

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState
    ) {
        int viewMargin = DpiUtils.DpToPixels(container.getContext(), 2);
        cameraViewLayoutParams = new LinearLayout.LayoutParams(
                0,
                LinearLayout.LayoutParams.MATCH_PARENT,
                1.0f
        );
        cameraViewLayoutParams.setMargins(viewMargin,viewMargin,viewMargin,viewMargin);

        rowLayoutParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT,
                1.0f
        );

        // 1,1 instead of 0,0 because the latter doesn't work on android 13+
        hiddenLayoutParams = new LinearLayout.LayoutParams(1, 1);

        binding = FragmentSurveillanceBinding.inflate(inflater, container, false);
        return binding.getRoot();
    }

    @Override
    public void onResume() {
        super.onResume();

        // Initialize performance monitoring
        performanceMonitor = PerformanceMonitor.getInstance(getContext());
        performanceMonitor.startTiming(PerformanceMonitor.Metrics.STREAM_START_TIME);
        performanceMonitor.logMemoryUsage("onResume");

        leanbackMode(true);

        fullscreenCameraView = false;
        addAllCameras();

        // Start playback for all streams
        for (CameraView cv : cameraViews) {
            cv.startPlayback();
        }

        expandToCameraViewIfRequired();

        // Register for back pressed events
        ((MainActivity)getActivity()).setOnBackButtonPressedListener(new OnBackButtonPressedListener() {
            @Override
            public boolean onBackPressed() {
                if(fullscreenCameraView && cameraViews.size() > 1) {
                    fullscreenCameraView = false;
                    showAllCameras();
                    return true;
                }
                return false;
            }
        });
    }

    /**
     * Goes fullscreen igoring the device screen insets (camera etc)
     */
    private void leanbackMode(boolean leanback) {
        Window w = requireActivity().getWindow();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P)
            return;

        if (leanback) {
            w.getAttributes().layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;

            // Hide system bar
            WindowInsetsControllerCompat windowInsetsController = WindowCompat.getInsetsController(w, w.getDecorView());
            windowInsetsController.hide(WindowInsetsCompat.Type.systemBars());
            // System bar is hidden when not touched for a while
            windowInsetsController.setSystemBarsBehavior(WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
        } else {
            // Show system bar
            //WindowInsetsControllerCompat windowInsetsController = WindowCompat.getInsetsController(w, w.getDecorView());
            //windowInsetsController.show(WindowInsetsCompat.Type.systemBars());
        }
    }

    @Override
    public void onPause() {
        super.onPause();

        leanbackMode(false);

        // Pause all players before disposing to save resources
        for (CameraView cv : cameraViews) {
            cv.pausePlayback();
        }

        disposeAllCameras();

        // Log performance metrics and cleanup
        if (performanceMonitor != null) {
            performanceMonitor.logMemoryUsage("onPause");
            performanceMonitor.generatePerformanceReport();
        }
    }


    private void addAllCameras() {
        Settings settings = Settings.fromDisk(getContext());
        List<Camera> cc = settings.getCameras();

        // If no cameras configured, add test streams for debugging
        if (cc.isEmpty()) {
            Log.d(TAG, "No cameras configured, adding test streams for debugging");
            cc = new ArrayList<>();
            for (int i = 0; i < TEST_STREAMS.length; i++) {
                cc.add(new Camera("Test Stream " + (i + 1), TEST_STREAMS[i]));
            }
        }

        // Limit concurrent streams for performance
        if (cc.size() > MAX_CONCURRENT_STREAMS) {
            Log.w(TAG, "Too many cameras (" + cc.size() + "), limiting to " + MAX_CONCURRENT_STREAMS + " for performance");
            cc = cc.subList(0, MAX_CONCURRENT_STREAMS);
        }

        int[] gridSize = calcGridDimensionsBasedOnNumberOfElements(cc.size());
        int camIdx = 0;
        for (int r = 0; r < gridSize[0]; r++) {
            // Create row and add to row container
            LinearLayout row = new LinearLayout(getContext());
            binding.gridRowContainer.addView(row, rowLayoutParams);
            // Add camera viewers to the row
            for (int c = 0; c < gridSize[1]; c++) {
                if ( camIdx < cc.size() ) {
                    Camera cam = cc.get(camIdx);
                    CameraView cv = addCameraView(cam, row);
                    cv.startPlayback();
                    cv.setOnClickListener(new View.OnClickListener() {
                        @Override
                        public void onClick(View v) {
                            // Toggle single/multi camera views
                            fullscreenCameraView = !fullscreenCameraView;
                            if (fullscreenCameraView) {
                                hideAllCameraViewsButNot(v);
                            } else {
                                showAllCameras();
                            }
                        }
                    });
                } else {
                    // Cameras are less than the maximum number of cells in grid: fill remaining cells with empty views
                    View ev = new View(getContext());
                    ev.setBackgroundColor(getResources().getColor(R.color.purple_700));
                    row.addView(ev, cameraViewLayoutParams);
                }
                camIdx++;
            }
        }
    }

    private void disposeAllCameras() {
        // Destroy players, libs etc
        for (CameraView cv : cameraViews) {
            cv.destroy();
        }
        cameraViews.clear();
        // Remove views
        binding.gridRowContainer.removeAllViews();
    }

    protected void hideAllCameraViewsButNot(View cameraView) {
        for (int i = 0; i < binding.gridRowContainer.getChildCount(); i++) {
            LinearLayout row = (LinearLayout) binding.gridRowContainer.getChildAt(i);
            boolean emptyRow = true;
            for (int j = 0; j < row.getChildCount(); j++) {
                View cam = row.getChildAt(j);
                if (cameraView == cam)
                    emptyRow = false;
                else
                    cam.setLayoutParams(hiddenLayoutParams);
            }
            if (emptyRow)
                row.setLayoutParams(hiddenLayoutParams);
        }
    }

    protected void showAllCameras() {
        for (int i = 0; i < binding.gridRowContainer.getChildCount(); i++) {
            LinearLayout row = (LinearLayout) binding.gridRowContainer.getChildAt(i);
            row.setLayoutParams(rowLayoutParams);
            for (int j = 0; j < row.getChildCount(); j++) {
                View cam = row.getChildAt(j);
                cam.setLayoutParams(cameraViewLayoutParams);
            }
        }
    }

    private CameraView addCameraView(Camera camera, LinearLayout rowContainer) {
        CameraView cv = new CameraView(
                getContext(),
                camera
        );

        // Add to layout
        rowContainer.addView(cv.surfaceView, cameraViewLayoutParams);

        cameraViews.add(cv);
        return cv;
    }

    /**
     * Returns the dimensions of the grid based on the number of elements.
     * Es: to display 3 elements is needed a 4-element grid, with 2 elements per side (a 2x2 grid)
     * Es: to display 6 elements is needed a 9-element grid, with 3 elements per side (a 2x3 grid)
     * Es: to display 7 elements is needed a 9-element grid, with 3 elements per side (a 3x3 grid)
     * @param elements
     */
    private int[] calcGridDimensionsBasedOnNumberOfElements(int elements) {
        int rows = 1;
        int cols = 1;
        while (rows * cols < elements) {
            cols += 1;
            if (rows * cols >= elements) break;
            rows += 1;
        }
        int[] dimensions = {rows, cols};
        return dimensions;
    }

    private void expandToCameraViewIfRequired() {
        final String EXTRA_CAMERA_NUMBER = "it.danieleverducci.ojo.CAMERA_NUMBER";
        final String EXTRA_CAMERA_NAME = "it.danieleverducci.ojo.CAMERA_NAME";
        final String OPEN_CAMERA = "it.danieleverducci.ojo.OPEN_CAMERA";

        if (this.getActivity() == null) {
            return;
        }

        Intent intent = this.getActivity().getIntent();

        if (OPEN_CAMERA.equals(intent.getAction())) {
            String cameraName = intent.getStringExtra(EXTRA_CAMERA_NAME);
            if (cameraName == null) {
                int cameraNumber = intent.getIntExtra(EXTRA_CAMERA_NUMBER, 0) - 1;
                expandByIndex(cameraNumber);
                return;
            }
            expandByName(cameraName);
        }
    }

    private void expandByIndex(int index) {
        if (index < 0 || cameraViews.size() <= index) {
            return;
        }
        hideAllCameraViewsButNot(cameraViews.get(index).surfaceView);
    }

    private void expandByName(String name) {
        for(CameraView cameraView: cameraViews) {
            if (cameraView.camera.getName().equals(name)) {
                hideAllCameraViewsButNot(cameraView.surfaceView);
                break;
            }
        }
    }

    /**
     * Contains all entities (views and java entities) related to a camera stream viewer
     * Now using ExoPlayer instead of VLC for RTSP streaming
     */
    private class CameraView {
        protected SurfaceView surfaceView;
        protected ExoPlayer exoPlayer;
        protected Camera camera;
        protected MediaSource mediaSource;
        protected boolean isPlayerReady = false;
        protected int retryCount = 0;
        protected boolean isDestroyed = false;
        protected PlayerState currentState = PlayerState.IDLE;

        public CameraView(Context context, Camera camera) {
            this.camera = camera;

            // Start performance monitoring for camera initialization
            if (performanceMonitor != null) {
                performanceMonitor.startTiming(PerformanceMonitor.Metrics.CAMERA_INIT_TIME + "_" + camera.getName());
            }

            // Create SurfaceView for video rendering
            surfaceView = new SurfaceView(context);
            surfaceView.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    // Click handler will be set externally
                }
            });
            surfaceView.setOnFocusChangeListener((view, hasFocus) ->
                view.setBackgroundResource(hasFocus ? R.drawable.focus_border : 0));

            SurfaceHolder holder = surfaceView.getHolder();
            holder.setKeepScreenOn(true);

            // Add surface callback for proper lifecycle management
            holder.addCallback(new SurfaceHolder.Callback() {
                @Override
                public void surfaceCreated(SurfaceHolder holder) {
                    Log.d(TAG, "Surface created for camera: " + camera.getName());
                    // Surface is ready, ExoPlayer can use it
                    // Draw a test pattern to verify surface is working
                    drawTestPattern(holder);
                }

                @Override
                public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                    Log.d(TAG, "Surface changed for camera: " + camera.getName() +
                          " - " + width + "x" + height + ", format: " + format);
                    // Redraw test pattern on surface change
                    drawTestPattern(holder);
                }

                @Override
                public void surfaceDestroyed(SurfaceHolder holder) {
                    Log.d(TAG, "Surface destroyed for camera: " + camera.getName());
                    // Surface is being destroyed
                }
            });

            // Create ExoPlayer instance with performance optimizations
            exoPlayer = new ExoPlayer.Builder(context)
                    .setLoadControl(createOptimizedLoadControl())
                    .setRenderersFactory(createOptimizedRenderersFactory(context))
                    .build();

            // Set video surface
            exoPlayer.setVideoSurfaceView(surfaceView);

            // Create media source based on URL type
            DefaultDataSource.Factory dataSourceFactory = new DefaultDataSource.Factory(context);
            MediaItem mediaItem = MediaItem.fromUri(camera.getRtspUrl());

            try {
                String url = camera.getRtspUrl().toLowerCase();
                if (url.startsWith("rtsp://")) {
                    // Create RTSP media source with enhanced configuration
                    RtspMediaSource.Factory rtspSourceFactory = new RtspMediaSource.Factory()
                            .setForceUseRtpTcp(false)  // Allow UDP first, fallback to TCP
                            .setTimeoutMs(RTSP_TIMEOUT_MS);
                    mediaSource = rtspSourceFactory.createMediaSource(mediaItem);
                    Log.d(TAG, "Created RTSP media source for: " + camera.getRtspUrl());
                } else if (url.startsWith("http://") || url.startsWith("https://")) {
                    // Create progressive media source for HTTP streams
                    com.google.android.exoplayer2.source.ProgressiveMediaSource.Factory progressiveFactory =
                        new com.google.android.exoplayer2.source.ProgressiveMediaSource.Factory(dataSourceFactory);
                    mediaSource = progressiveFactory.createMediaSource(mediaItem);
                    Log.d(TAG, "Created HTTP media source for: " + camera.getRtspUrl());
                } else {
                    Log.e(TAG, "Unsupported URL scheme for: " + camera.getRtspUrl());
                    mediaSource = null;
                    return;
                }
            } catch (Exception e) {
                Log.e(TAG, "Failed to create media source for " + camera.getName() + ": " + e.getMessage());
                mediaSource = null;
                return;
            }

            // Set up player listener for error handling and state changes
            exoPlayer.addListener(new Player.Listener() {
                @Override
                public void onPlaybackStateChanged(int state) {
                    updatePlayerState(state);
                    isPlayerReady = (state == Player.STATE_READY);

                    String stateStr = getStateString(state);
                    Log.d(TAG, "Player state changed to " + stateStr + " for camera: " + camera.getName());

                    // Reset retry count on successful ready state
                    if (state == Player.STATE_READY) {
                        retryCount = 0;
                        currentState = PlayerState.READY;

                        // Log successful connection
                        if (performanceMonitor != null) {
                            performanceMonitor.incrementCounter(PerformanceMonitor.Metrics.SUCCESSFUL_CONNECTIONS);
                            performanceMonitor.endTiming(PerformanceMonitor.Metrics.CAMERA_INIT_TIME + "_" + camera.getName());
                            performanceMonitor.logPlayerPerformance(camera.getName(), "ready_state_reached", System.currentTimeMillis());
                        }
                    } else if (state == Player.STATE_BUFFERING) {
                        currentState = PlayerState.BUFFERING;
                    } else if (state == Player.STATE_ENDED) {
                        currentState = PlayerState.ENDED;
                        // Automatically restart if stream ends unexpectedly
                        scheduleRestart();
                    }
                }

                @Override
                public void onPlayerError(PlaybackException error) {
                    currentState = PlayerState.ERROR;

                    // Log detailed error information
                    String errorDetails = getDetailedErrorInfo(error);
                    Log.e(TAG, "ExoPlayer error for camera " + camera.getName() +
                          " (attempt " + (retryCount + 1) + "/" + MAX_RETRY_ATTEMPTS + "): " +
                          error.getMessage());
                    Log.e(TAG, "Error details: " + errorDetails);
                    Log.e(TAG, "RTSP URL: " + camera.getRtspUrl());

                    // Log error metrics
                    if (performanceMonitor != null) {
                        performanceMonitor.incrementCounter(PerformanceMonitor.Metrics.ERROR_COUNT);
                        performanceMonitor.incrementCounter(PerformanceMonitor.Metrics.FAILED_CONNECTIONS);
                        performanceMonitor.logPlayerPerformance(camera.getName(), "error_occurred", error.errorCode);
                    }

                    // Attempt to restart playback after error with retry limit
                    if (retryCount < MAX_RETRY_ATTEMPTS && !isDestroyed) {
                        scheduleRestart();
                    } else {
                        Log.e(TAG, "Max retry attempts reached for camera: " + camera.getName());
                        Log.e(TAG, "Final error details: " + errorDetails);
                    }
                }
            });

            // Prepare the player with media source
            if (mediaSource != null) {
                exoPlayer.setMediaSource(mediaSource);
                exoPlayer.prepare();
            } else {
                Log.e(TAG, "Cannot prepare player - media source is null for camera: " + camera.getName());
                currentState = PlayerState.ERROR;
            }
        }

        public void setOnClickListener(View.OnClickListener listener) {
            surfaceView.setOnClickListener(listener);
        }

        /**
         * Starts the playback.
         */
        public void startPlayback() {
            if (exoPlayer != null) {
                exoPlayer.setPlayWhenReady(true);
                Log.d(TAG, "Starting playback for camera: " + camera.getName());
            }
        }

        /**
         * Pauses the playback.
         */
        public void pausePlayback() {
            if (exoPlayer != null) {
                exoPlayer.setPlayWhenReady(false);
                Log.d(TAG, "Pausing playback for camera: " + camera.getName());
            }
        }

        /**
         * Schedules a restart with delay to avoid rapid retry loops.
         */
        private void scheduleRestart() {
            if (isDestroyed) return;

            retryCount++;
            Log.d(TAG, "Scheduling restart for camera: " + camera.getName() +
                  " (attempt " + retryCount + "/" + MAX_RETRY_ATTEMPTS + ")");

            // Use handler to delay restart
            surfaceView.postDelayed(this::restartPlayback, RETRY_DELAY_MS);
        }

        /**
         * Restarts playback after an error or connection issue.
         */
        private void restartPlayback() {
            if (exoPlayer != null && !isDestroyed) {
                Log.d(TAG, "Restarting playback for camera: " + camera.getName());
                currentState = PlayerState.PREPARING;

                try {
                    exoPlayer.stop();
                    exoPlayer.setMediaSource(mediaSource);
                    exoPlayer.prepare();
                    exoPlayer.setPlayWhenReady(true);
                } catch (Exception e) {
                    Log.e(TAG, "Error during restart for camera " + camera.getName() + ": " + e.getMessage());
                    currentState = PlayerState.ERROR;
                }
            }
        }

        /**
         * Updates internal player state tracking.
         */
        private void updatePlayerState(int exoPlayerState) {
            switch (exoPlayerState) {
                case Player.STATE_IDLE:
                    currentState = PlayerState.IDLE;
                    break;
                case Player.STATE_BUFFERING:
                    currentState = PlayerState.BUFFERING;
                    break;
                case Player.STATE_READY:
                    currentState = PlayerState.READY;
                    break;
                case Player.STATE_ENDED:
                    currentState = PlayerState.ENDED;
                    break;
            }
        }

        /**
         * Converts ExoPlayer state to readable string.
         */
        private String getStateString(int state) {
            switch (state) {
                case Player.STATE_IDLE: return "IDLE";
                case Player.STATE_BUFFERING: return "BUFFERING";
                case Player.STATE_READY: return "READY";
                case Player.STATE_ENDED: return "ENDED";
                default: return "UNKNOWN";
            }
        }

        /**
         * Gets detailed error information for debugging.
         */
        private String getDetailedErrorInfo(PlaybackException error) {
            StringBuilder details = new StringBuilder();
            details.append("Error Code: ").append(error.errorCode).append(", ");
            details.append("Type: ");

            switch (error.errorCode) {
                case PlaybackException.ERROR_CODE_IO_NETWORK_CONNECTION_FAILED:
                    details.append("Network connection failed");
                    break;
                case PlaybackException.ERROR_CODE_IO_NETWORK_CONNECTION_TIMEOUT:
                    details.append("Network connection timeout");
                    break;
                case PlaybackException.ERROR_CODE_PARSING_CONTAINER_MALFORMED:
                    details.append("Malformed container/SDP");
                    break;
                case PlaybackException.ERROR_CODE_PARSING_MANIFEST_MALFORMED:
                    details.append("Malformed manifest");
                    break;
                default:
                    details.append("Unknown (").append(error.errorCode).append(")");
            }

            if (error.getCause() != null) {
                details.append(", Cause: ").append(error.getCause().getMessage());
            }

            return details.toString();
        }

        /**
         * Destroys the object and frees the memory
         */
        public void destroy() {
            if (isDestroyed || exoPlayer == null) {
                Log.e(TAG, this.toString() + " already destroyed");
                return;
            }

            Log.d(TAG, "Destroying camera view for: " + camera.getName());
            isDestroyed = true;
            currentState = PlayerState.IDLE;

            // Cancel any pending restart operations
            surfaceView.removeCallbacks(this::restartPlayback);

            try {
                exoPlayer.stop();
                exoPlayer.release();
            } catch (Exception e) {
                Log.e(TAG, "Error during player cleanup for " + camera.getName() + ": " + e.getMessage());
            } finally {
                exoPlayer = null;
                mediaSource = null;
            }
        }

        /**
         * Gets current connection status for monitoring.
         */
        public boolean isConnected() {
            return currentState == PlayerState.READY && isPlayerReady;
        }

        /**
         * Gets current player state for debugging.
         */
        public PlayerState getCurrentState() {
            return currentState;
        }

        /**
         * Creates optimized LoadControl for better performance and memory usage.
         */
        private LoadControl createOptimizedLoadControl() {
            return new DefaultLoadControl.Builder()
                    .setBufferDurationsMs(
                            (int) MIN_BUFFER_MS,     // Min buffer before playback starts
                            (int) BUFFER_SIZE_MS,    // Max buffer size
                            (int) MIN_BUFFER_MS,     // Buffer for playback after rebuffer
                            (int) MIN_BUFFER_MS      // Buffer for playback after rebuffer
                    )
                    .setPrioritizeTimeOverSizeThresholds(true) // Prioritize time over size
                    .build();
        }

        /**
         * Creates optimized RenderersFactory for hardware acceleration.
         */
        private DefaultRenderersFactory createOptimizedRenderersFactory(Context context) {
            DefaultRenderersFactory factory = new DefaultRenderersFactory(context);

            if (ENABLE_HARDWARE_ACCELERATION) {
                // Enable hardware acceleration when available
                factory.setExtensionRendererMode(DefaultRenderersFactory.EXTENSION_RENDERER_MODE_PREFER);
            } else {
                // Use software rendering only
                factory.setExtensionRendererMode(DefaultRenderersFactory.EXTENSION_RENDERER_MODE_OFF);
            }

            return factory;
        }

        /**
         * Draws a test pattern on the surface to verify rendering is working.
         */
        private void drawTestPattern(SurfaceHolder holder) {
            try {
                android.graphics.Canvas canvas = holder.lockCanvas();
                if (canvas != null) {
                    // Clear with a color based on camera name
                    int color = camera.getName().hashCode() | 0xFF000000; // Ensure alpha is set
                    canvas.drawColor(color);

                    // Draw some text
                    android.graphics.Paint paint = new android.graphics.Paint();
                    paint.setColor(android.graphics.Color.WHITE);
                    paint.setTextSize(48);
                    paint.setAntiAlias(true);

                    String text = "Test: " + camera.getName();
                    float x = canvas.getWidth() / 2 - paint.measureText(text) / 2;
                    float y = canvas.getHeight() / 2;
                    canvas.drawText(text, x, y, paint);

                    // Draw a border
                    paint.setStyle(android.graphics.Paint.Style.STROKE);
                    paint.setStrokeWidth(5);
                    canvas.drawRect(5, 5, canvas.getWidth() - 5, canvas.getHeight() - 5, paint);

                    holder.unlockCanvasAndPost(canvas);
                    Log.d(TAG, "Drew test pattern for camera: " + camera.getName());
                }
            } catch (Exception e) {
                Log.e(TAG, "Error drawing test pattern: " + e.getMessage());
            }
        }
    }
}