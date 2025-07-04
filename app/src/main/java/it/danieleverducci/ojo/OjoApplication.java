package it.danieleverducci.ojo;

import androidx.multidex.MultiDexApplication;

/**
 * Custom Application class to support MultiDex for ExoPlayer
 */
public class OjoApplication extends MultiDexApplication {
    
    @Override
    public void onCreate() {
        super.onCreate();
        // Application initialization code can be added here if needed
    }
}
