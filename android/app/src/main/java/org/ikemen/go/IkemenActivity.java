package org.ikemen.go;

import android.os.Build;
import android.os.Bundle;
import android.view.WindowManager;
import org.libsdl.app.SDLActivity;

public class IkemenActivity extends SDLActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if (Build.VERSION.SDK_INT >= 28) {
            getWindow().getAttributes().layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }
    }

    @Override
    protected String[] getLibraries() {
        return new String[]{
            "c++_shared",
            "SDL2",
            "ikemen_cpp"
        };
    }
}

