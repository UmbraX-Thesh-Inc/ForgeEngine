package com.forgeengine;

// ============================================================
//  ForgeEngine  –  ForgeJMEApp.java
//  The JMonkeyEngine SimpleApplication that powers the editor.
// ============================================================

import com.jme3.app.SimpleApplication;
import com.jme3.input.FlyByCamera;
import com.jme3.light.AmbientLight;
import com.jme3.light.DirectionalLight;
import com.jme3.math.ColorRGBA;
import com.jme3.math.Vector3f;
import com.jme3.scene.shape.Box;
import com.jme3.scene.Geometry;
import com.jme3.material.Material;
import com.jme3.renderer.RenderManager;
import com.jme3.system.AppSettings;

public class ForgeJMEApp extends SimpleApplication {

    private final EditorBridge  mBridge;
    private final ForgeCommandStack mCmdStack = new ForgeCommandStack();
    private boolean             mEditorMode = true;
    private boolean             mIs3D       = true;

    public ForgeJMEApp(EditorBridge bridge) {
        mBridge = bridge;
    }

    // ─────────────────────────────────────────────────────────
    @Override
    public void simpleInitApp() {
        // Disable default fly-cam in editor
        flyCam.setEnabled(false);

        // Editor ambient scene setup
        AmbientLight ambient = new AmbientLight();
        ambient.setColor(ColorRGBA.White.mult(0.4f));
        rootNode.addLight(ambient);

        DirectionalLight sun = new DirectionalLight();
        sun.setDirection(new Vector3f(-0.5f,-1f,-0.5f).normalizeLocal());
        sun.setColor(ColorRGBA.White);
        rootNode.addLight(sun);

        // Default editor grid geometry (drawn via DebugGeom in real impl)
        viewPort.setBackgroundColor(new ColorRGBA(0.08f, 0.09f, 0.12f, 1f));

        mBridge.log("JME", "Scene initialized");
    }

    // ─────────────────────────────────────────────────────────
    @Override
    public void simpleUpdate(float tpf) {
        if (!mEditorMode) {
            // Game running: update game logic
        }
        // Poll editor camera pan/zoom from ImGui gizmo input
    }

    @Override
    public void simpleRender(RenderManager rm) {
        // Additional render passes (grid, gizmos, etc.)
    }

    // ─────────────────────────────────────────────────────────
    public void setEditorMode(boolean editor) { mEditorMode = editor; }
    public void setIs3D(boolean is3D)         { mIs3D = is3D; }
    public ForgeCommandStack getCommandStack() { return mCmdStack; }
}
