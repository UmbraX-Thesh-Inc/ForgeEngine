package com.forgeengine;

// ============================================================
//  ForgeEngine – ForgeActivity.java
//  Main Android Activity: bootstraps JME + native ImGui editor.
//  Handles lifecycle, touch/key input routing.
// ============================================================

import android.app.Activity;
import android.os.Bundle;
import android.view.*;
import android.view.inputmethod.InputMethodManager;
import android.content.Context;

import com.jme3.system.android.JmeAndroidSystem;
import com.jme3.system.android.OGLESContext;

import com.forgeengine.editor.EditorBridgeFull;
import com.forgeengine.input.ForgeInputManager;

public class ForgeActivity extends Activity {

    private EditorBridgeFull  mBridge;
    private ForgeInputManager mInput;
    private GLSurfaceView     mGLView;
    private int               mScreenW, mScreenH;

    // Native methods
    private native void nativeOnCreate(Activity activity, String assetPath, String iconsPath);
    private native void nativeOnResume();
    private native void nativePause();
    private native void nativeOnStop();
    private native void nativeOnSurfaceCreated(Surface surface, int w, int h);
    private native void nativeOnSurfaceChanged(int w, int h);
    private native void nativeTouchEvent(int action, int ptrId, float x, float y, float pressure);
    private native void nativeKeyDown(int keyCode);
    private native void nativeKeyUp(int keyCode);
    private native void nativeBackButton();

    static {
        // Load native libs in dependency order
        System.loadLibrary("c++_shared");
        System.loadLibrary("bullet_native");   // JME bullet
        System.loadLibrary("openal");          // OpenAL
        System.loadLibrary("forge_editor");    // Our native lib (CMakeLists target)
    }

    // ─────────────────────────────────────────────────────────
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Fullscreen
        getWindow().setFlags(
            WindowManager.LayoutParams.FLAG_FULLSCREEN,
            WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().addFlags(
            WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        mInput  = new ForgeInputManager();
        mBridge = new EditorBridgeFull(this);

        // Get paths
        String assetPath = getFilesDir().getAbsolutePath();
        String iconsPath = assetPath + "/icons";

        // Copy bundled assets on first run
        copyAssetsIfNeeded(assetPath);

        // Init native editor (boots JME + ImGui)
        nativeOnCreate(this, assetPath, iconsPath);

        // Create OpenGL surface
        mGLView = createGLView();
        setContentView(mGLView);
    }

    private GLSurfaceView createGLView() {
        GLSurfaceView view = new GLSurfaceView(this) {
            @Override
            public boolean onTouchEvent(MotionEvent event) {
                int action = event.getActionMasked();
                int ptrIdx = event.getActionIndex();
                int ptrId  = event.getPointerId(ptrIdx);
                float x    = event.getX(ptrIdx);
                float y    = event.getY(ptrIdx);
                float pres = event.getPressure(ptrIdx);
                nativeTouchEvent(action, ptrId, x, y, pres);
                // Also route to ForgeInputManager for virtual joystick
                mInput.onTouchEvent(event, mScreenW, mScreenH);
                return true;
            }
            @Override
            public boolean onKeyDown(int keyCode, KeyEvent event) {
                nativeKeyDown(keyCode);
                mInput.onKeyDown(keyCode, event);
                return super.onKeyDown(keyCode, event);
            }
            @Override
            public boolean onKeyUp(int keyCode, KeyEvent event) {
                nativeKeyUp(keyCode);
                mInput.onKeyUp(keyCode, event);
                return super.onKeyUp(keyCode, event);
            }
            @Override
            public boolean onGenericMotionEvent(MotionEvent event) {
                mInput.onGenericMotionEvent(event);
                return super.onGenericMotionEvent(event);
            }
        };

        view.setEGLContextClientVersion(3); // OpenGL ES 3.0
        view.setPreserveEGLContextOnPause(true);
        view.setFocusable(true);
        view.setFocusableInTouchMode(true);

        view.setRenderer(new GLSurfaceView.Renderer() {
            @Override public void onSurfaceCreated(
                    javax.microedition.khronos.egl.EGLConfig config) {
                // Surface ready
            }
            @Override public void onSurfaceChanged(
                    javax.microedition.khronos.egl.EGL10 egl,
                    javax.microedition.khronos.egl.EGLDisplay display,
                    int w, int h) {
                mScreenW = w; mScreenH = h;
                nativeOnSurfaceChanged(w, h);
            }
            @Override public void onSurfaceChanged(
                    javax.microedition.khronos.opengles.GL10 gl, int w, int h) {
                mScreenW = w; mScreenH = h;
                nativeOnSurfaceChanged(w, h);
            }
            @Override public void onDrawFrame(
                    javax.microedition.khronos.opengles.GL10 gl) {
                // Render is driven by native main loop
            }
        });

        return view;
    }

    // ── Lifecycle ─────────────────────────────────────────────
    @Override protected void onResume() {
        super.onResume();
        mGLView.onResume();
        nativeOnResume();
    }
    @Override protected void onPause() {
        super.onPause();
        mGLView.onPause();
        nativePause();
    }
    @Override protected void onStop() {
        super.onStop();
        nativeOnStop();
    }
    @Override protected void onDestroy() {
        super.onDestroy();
    }
    @Override public void onBackPressed() {
        nativeBackButton();
    }
    @Override public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        }
    }

    // ── Asset copy helper ─────────────────────────────────────
    private void copyAssetsIfNeeded(String destBase) {
        try {
            String[] prefixes = {"icons/svg", "icons/png", "shaders", "matdefs"};
            for (String prefix : prefixes) {
                try {
                    String[] files = getAssets().list(prefix);
                    if (files == null) continue;
                    java.io.File dir = new java.io.File(destBase + "/" + prefix);
                    dir.mkdirs();
                    for (String f : files) {
                        java.io.File dest = new java.io.File(dir, f);
                        if (dest.exists()) continue;
                        try (java.io.InputStream in = getAssets().open(prefix+"/"+f);
                             java.io.FileOutputStream out = new java.io.FileOutputStream(dest)) {
                            byte[] buf = new byte[4096]; int n;
                            while ((n=in.read(buf))>0) out.write(buf,0,n);
                        }
                    }
                } catch (Exception ignored) {}
            }
        } catch (Exception ignored) {}
    }

    // ── EditorBridge accessor (called from native via JNI) ────
    public EditorBridgeFull getBridge() { return mBridge; }
    public ForgeInputManager getInputManager() { return mInput; }

    // ── Show soft keyboard ────────────────────────────────────
    public void showKeyboard(boolean show) {
        InputMethodManager imm = (InputMethodManager)
            getSystemService(Context.INPUT_METHOD_SERVICE);
        if (show)
            imm.showSoftInput(mGLView, InputMethodManager.SHOW_FORCED);
        else
            imm.hideSoftInputFromWindow(mGLView.getWindowToken(), 0);
    }
}
