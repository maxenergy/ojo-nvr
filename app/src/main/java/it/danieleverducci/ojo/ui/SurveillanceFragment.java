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
import com.google.android.exoplayer2.source.rtsp.RtspMediaSource;
import com.google.android.exoplayer2.ui.PlayerView;
import com.google.android.exoplayer2.upstream.DefaultDataSource;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import it.danieleverducci.ojo.R;
import it.danieleverducci.ojo.Settings;
import it.danieleverducci.ojo.databinding.FragmentSurveillanceBinding;
import it.danieleverducci.ojo.entities.Camera;
import it.danieleverducci.ojo.utils.DpiUtils;

/**
 * Some streams to test:
 * rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov
 * rtsp://demo:demo@ipvmdemo.dyndns.org:5541/onvif-media/media.amp?profile=profile_1_h264&sessiontimeout=60&streamtype=unicast
 */
public class SurveillanceFragment extends Fragment {

    final static private String TAG = "SurveillanceFragment";

    // ExoPlayer configuration constants
    private static final long RTSP_TIMEOUT_MS = 10000; // 10 seconds timeout
    private static final boolean ENABLE_AUDIO = true;  // Enable audio for RTSP streams
    private static final int MAX_RETRY_ATTEMPTS = 3;   // Maximum retry attempts on error
    private static final long RETRY_DELAY_MS = 5000;   // Delay between retry attempts

    private FragmentSurveillanceBinding binding;
    private List<CameraView> cameraViews = new ArrayList<>();
    private boolean fullscreenCameraView = false;
    private LinearLayout.LayoutParams cameraViewLayoutParams;
    private LinearLayout.LayoutParams rowLayoutParams;
    private LinearLayout.LayoutParams hiddenLayoutParams;

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
    }


    private void addAllCameras() {
        Settings settings = Settings.fromDisk(getContext());
        List<Camera> cc = settings.getCameras();

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

        // Player states for better error handling
        protected enum PlayerState {
            IDLE, PREPARING, READY, BUFFERING, ERROR, ENDED
        }
        protected PlayerState currentState = PlayerState.IDLE;

        public CameraView(Context context, Camera camera) {
            this.camera = camera;

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

            // Create ExoPlayer instance
            exoPlayer = new ExoPlayer.Builder(context).build();

            // Set video surface
            exoPlayer.setVideoSurfaceView(surfaceView);

            // Create RTSP media source
            DefaultDataSource.Factory dataSourceFactory = new DefaultDataSource.Factory(context);
            RtspMediaSource.Factory rtspSourceFactory = new RtspMediaSource.Factory(dataSourceFactory)
                    .setTimeoutMs(RTSP_TIMEOUT_MS);

            MediaItem mediaItem = MediaItem.fromUri(camera.getRtspUrl());
            mediaSource = rtspSourceFactory.createMediaSource(mediaItem);

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
                    Log.e(TAG, "ExoPlayer error for camera " + camera.getName() +
                          " (attempt " + (retryCount + 1) + "/" + MAX_RETRY_ATTEMPTS + "): " +
                          error.getMessage());

                    // Attempt to restart playback after error with retry limit
                    if (retryCount < MAX_RETRY_ATTEMPTS && !isDestroyed) {
                        scheduleRestart();
                    } else {
                        Log.e(TAG, "Max retry attempts reached for camera: " + camera.getName());
                    }
                }
            });

            // Prepare the player with media source
            exoPlayer.setMediaSource(mediaSource);
            exoPlayer.prepare();
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
    }
}