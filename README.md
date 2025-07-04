![Ojo Logo](/media/icon.png)

# Ojo: the FLOSS RTSP Surveillance camera viewer for Android

[<img src="https://raw.githubusercontent.com/andOTP/andOTP/master/assets/badges/get-it-on-github.png" height="80">](https://github.com/penguin86/ojo/releases/latest) 
[<img src="https://fdroid.gitlab.io/artwork/badge/get-it-on.png" alt="Get it on F-Droid" height="80">](https://f-droid.org/it/packages/it.danieleverducci.ojo)

(Always prefer [F-Droid](https://f-droid.org) build, when possible).

Ojo is a basic IP Camera surveillance wall.
IP camera's RTSP streams are added via its url and shown in the classic tile configuration. The number of tiles is automatically choosen based on the number of configured cameras: a single camera goes full screen, adding more cameras the app switches to a grid view: 2x2, 3x3, 4x4 and so on.
The maximum number of cameras is determined by the device's capabilities.

The stream decoding and rendering is powered by [Google's ExoPlayer](https://github.com/google/ExoPlayer), providing robust RTSP streaming capabilities with excellent performance and reliability.
This app was specifically developed for F-Droid, as I couldn't find any open source RTSP viewers in the main repository.

**Note**: This version has been migrated from VLC to ExoPlayer for improved performance, better error handling, and enhanced stability.

The app can be opened deeplinking to url ojo://view.
To open the app with focus on a specific camera, you can use an intent (it.danieleverducci.ojo.OPEN_CAMERA) to specify which camera you want to view.
The extra argument it.danieleverducci.ojo.CAMERA_NAME will open the app with the camera with the name you specified while adding the camera.
The extra argument it.danieleverducci.ojo.CAMERA_NUMBER starting at 1 could be used as well, if you have multiple cameras with the same name.
See belows example how to use the intent. The flag (-f 268468224) could be useful if you want to switch to an other camera while the app is running.
```shell
adb -s <YOUR_DEVICE> shell am start -a it.danieleverducci.ojo.OPEN_CAMERA -f 268468224 --es it.danieleverducci.ojo.CAMERA_NAME <YOUR_CAMERA_NAME>
adb -s <YOUR_DEVICE> shell am start -a it.danieleverducci.ojo.OPEN_CAMERA -f 268468224 --es it.danieleverducci.ojo.CAMERA_NUMBER <YOUR_CAMERA_NUMBER>
```


![Screenshot 1](media/screenshots/1.png)      ![Screenshot 2](media/screenshots/2.png)      ![Screenshot 3](media/screenshots/3.png)

## Technical Details

### ExoPlayer Migration
This version has been migrated from VLC to Google's ExoPlayer for improved performance and reliability:

- **Better Performance**: Optimized buffer management and hardware acceleration support
- **Enhanced Error Handling**: Automatic retry mechanisms with configurable limits
- **Improved Stability**: Better resource management and memory usage optimization
- **Modern Architecture**: Built on Google's recommended media playback framework

### System Requirements
- **Minimum Android Version**: Android 4.1 (API 16) - updated from API 15
- **Recommended RAM**: 2GB or more for optimal multi-camera performance
- **Network**: Stable internet connection for RTSP streaming

### Performance Optimizations
- Concurrent stream limit (4 cameras) for optimal performance
- Hardware-accelerated video decoding when available
- Intelligent buffer management for reduced memory usage
- Automatic quality adjustment based on device capabilities

## Contributors
- Thanks to [brenard](https://github.com/brenard) for the new grid sizing method
- Thanks to [davquar](https://github.com/davquar) for the fullscreen compatibility fix on Android 11
- Thanks to [jayfan0](https://github.com/jayfan0) for the first deep link implementation
- Thanks to [free-bots](https://github.com/free-bots) for the selection border on Android TV, intents for direct camera access and leanback support
