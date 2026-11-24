package com.vascolet.collections;

import android.os.Build;
import android.view.Display;
import android.view.Window;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

public class VascoletActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    // Gráficos > 120 FPS: pide el modo de pantalla con más Hz (on) o uno de 60 Hz (off).
    // Lo llama el código nativo (plat::setHighFps).
    public static void setHighRefresh(final boolean on) {
        final SDLActivity a = mSingleton;
        if (a == null || Build.VERSION.SDK_INT < 23) return;
        a.runOnUiThread(new Runnable() {
            @Override
            @android.annotation.TargetApi(23)
            public void run() {
                try {
                    Window w = a.getWindow();
                    Display d = a.getWindowManager().getDefaultDisplay();
                    Display.Mode cur = d.getMode();
                    int bestId = 0;
                    float bestHz = 0;
                    for (Display.Mode m : d.getSupportedModes()) {
                        if (m.getPhysicalWidth() != cur.getPhysicalWidth() || m.getPhysicalHeight() != cur.getPhysicalHeight()) continue;
                        float hz = m.getRefreshRate();
                        boolean better = on ? hz > bestHz : (bestId == 0 || Math.abs(hz - 60f) < Math.abs(bestHz - 60f));
                        if (better) { bestId = m.getModeId(); bestHz = hz; }
                    }
                    WindowManager.LayoutParams lp = w.getAttributes();
                    lp.preferredDisplayModeId = bestId;
                    lp.preferredRefreshRate = bestHz;
                    w.setAttributes(lp);
                } catch (Throwable ignored) {
                }
            }
        });
    }
}
