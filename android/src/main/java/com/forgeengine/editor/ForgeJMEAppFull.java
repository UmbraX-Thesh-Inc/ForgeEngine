package com.forgeengine.editor;

// ============================================================
//  ForgeEngine – ForgeJMEAppFull.java
//  Complete JMonkeyEngine application integrating:
//    • BulletAppState (physics)
//    • AudioRenderer (OpenAL)
//    • Post-processing filters (Bloom, SSAO, FXAA, Depth of Field)
//    • Environment probes (PBR IBL)
//    • Editor grid + gizmo geometry
//    • Editor camera control
//    • Debug shapes
// ============================================================

import com.jme3.app.SimpleApplication;
import com.jme3.bullet.BulletAppState;
import com.jme3.environment.EnvironmentCamera;
import com.jme3.environment.LightProbeFactory;
import com.jme3.environment.generation.JobProgressAdapter;
import com.jme3.light.*;
import com.jme3.material.Material;
import com.jme3.math.*;
import com.jme3.post.*;
import com.jme3.post.filters.*;
import com.jme3.renderer.RenderManager;
import com.jme3.renderer.ViewPort;
import com.jme3.scene.*;
import com.jme3.scene.debug.Grid;
import com.jme3.scene.shape.*;
import com.jme3.shadow.*;
import com.jme3.system.AppSettings;
import com.jme3.util.TangentBinormalGenerator;

import com.forgeengine.physics.PhysicsManager;

import java.util.concurrent.*;

public class ForgeJMEAppFull extends SimpleApplication {

    private final EditorBridgeFull mBridge;
    private final ForgeCommandStack mCmdStack = new ForgeCommandStack();

    // ── State ──────────────────────────────────────────────────
    private boolean     mEditorMode = true;
    private boolean     mIs3D       = true;
    private boolean     mShowGrid   = true;
    private boolean     mShowStats  = false;

    // ── Physics ───────────────────────────────────────────────
    private BulletAppState mBulletState;

    // ── Post-processing ───────────────────────────────────────
    private FilterPostProcessor mFPP;
    private BloomFilter         mBloom;
    private SSAOFilter          mSSAO;
    private FXAAFilter          mFXAA;
    private DepthOfFieldFilter  mDOF;
    private ColorOverlayFilter  mColorGrading;

    // ── Shadows ───────────────────────────────────────────────
    private DirectionalLightShadowRenderer mShadowRenderer;
    private DirectionalLight               mSunLight;

    // ── Editor grid ───────────────────────────────────────────
    private Geometry mGridGeom;
    private Node     mGizmoNode;

    // ── Env probe ─────────────────────────────────────────────
    private LightProbe  mEnvProbe;

    // ─────────────────────────────────────────────────────────
    public ForgeJMEAppFull(EditorBridgeFull bridge) {
        this.mBridge = bridge;
    }

    // ═══════════════════════════════════════════════════════
    //  Init
    // ═══════════════════════════════════════════════════════
    @Override
    public void simpleInitApp() {
        // ── Physics ──────────────────────────────────────────
        mBulletState = new BulletAppState();
        stateManager.attach(mBulletState);
        mBulletState.getPhysicsSpace().setGravity(new Vector3f(0,-9.81f,0));

        // ── Camera ───────────────────────────────────────────
        flyCam.setEnabled(false);
        cam.setLocation(new Vector3f(5, 5, 5));
        cam.lookAt(Vector3f.ZERO, Vector3f.UNIT_Y);
        cam.setFrustumPerspective(60f, (float)cam.getWidth()/cam.getHeight(), 0.1f, 2000f);

        // ── Lighting ─────────────────────────────────────────
        setupLighting();

        // ── Post-processing ──────────────────────────────────
        setupPostProcessing();

        // ── Editor grid ──────────────────────────────────────
        setupGrid();

        // ── Shadows ──────────────────────────────────────────
        setupShadows();

        // ── Environment probe (IBL for PBR) ──────────────────
        setupEnvironmentProbe();

        viewPort.setBackgroundColor(new ColorRGBA(0.08f,0.09f,0.12f,1f));

        mBridge.log("JME", "ForgeJMEAppFull initialized");
    }

    // ─────────────────────────────────────────────────────────
    private void setupLighting() {
        // Ambient
        AmbientLight ambient = new AmbientLight();
        ambient.setColor(ColorRGBA.White.mult(0.3f));
        rootNode.addLight(ambient);

        // Sun (directional)
        mSunLight = new DirectionalLight();
        mSunLight.setDirection(new Vector3f(-0.5f,-1f,-0.5f).normalizeLocal());
        mSunLight.setColor(ColorRGBA.White.mult(1.2f));
        rootNode.addLight(mSunLight);

        // Sky fill
        DirectionalLight sky = new DirectionalLight();
        sky.setDirection(new Vector3f(0.5f,1f,0.5f).normalizeLocal());
        sky.setColor(new ColorRGBA(0.3f,0.4f,0.8f,1f).mult(0.4f));
        rootNode.addLight(sky);
    }

    // ─────────────────────────────────────────────────────────
    private void setupPostProcessing() {
        mFPP = new FilterPostProcessor(assetManager);

        // SSAO
        mSSAO = new SSAOFilter(12f,0.4f,0.5f,0.02f);
        mFPP.addFilter(mSSAO);

        // Bloom
        mBloom = new BloomFilter(BloomFilter.GlowMode.SceneAndObjects);
        mBloom.setBloomIntensity(0.4f);
        mBloom.setExposurePower(3.5f);
        mFPP.addFilter(mBloom);

        // Depth of Field
        mDOF = new DepthOfFieldFilter();
        mDOF.setFocusDistance(5f);
        mDOF.setFocusRange(4f);
        mDOF.setBlurScale(1.5f);
        mDOF.setEnabled(false); // off by default
        mFPP.addFilter(mDOF);

        // FXAA (anti-aliasing)
        mFXAA = new FXAAFilter();
        mFPP.addFilter(mFXAA);

        viewPort.addProcessor(mFPP);
    }

    // ─────────────────────────────────────────────────────────
    private void setupShadows() {
        mShadowRenderer = new DirectionalLightShadowRenderer(assetManager, 2048, 3);
        mShadowRenderer.setLight(mSunLight);
        mShadowRenderer.setShadowIntensity(0.6f);
        mShadowRenderer.setShadowZExtend(150f);
        viewPort.addProcessor(mShadowRenderer);

        // Shadows on root node
        DirectionalLightShadowFilter shadowFilter =
            new DirectionalLightShadowFilter(assetManager, 2048, 3);
        shadowFilter.setLight(mSunLight);
        shadowFilter.setEnabled(true);
        mFPP.addFilter(shadowFilter);
    }

    // ─────────────────────────────────────────────────────────
    private void setupGrid() {
        mGizmoNode = new Node("GizmoNode");
        rootNode.attachChild(mGizmoNode);

        // XZ grid (20x20 cells, 1 unit spacing)
        Grid grid = new Grid(21, 21, 1f);
        mGridGeom = new Geometry("EditorGrid", grid);
        Material gridMat = new Material(assetManager, "Common/MatDefs/Misc/Unshaded.j3md");
        gridMat.setColor("Color", new ColorRGBA(0.3f,0.35f,0.4f,0.5f));
        gridMat.getAdditionalRenderState().setBlendMode(
            com.jme3.material.RenderState.BlendMode.Alpha);
        mGridGeom.setMaterial(gridMat);
        // Centre grid
        mGridGeom.setLocalTranslation(-10, 0, -10);
        mGizmoNode.attachChild(mGridGeom);

        // X axis line (red)
        addAxisLine(new Vector3f(-50,0,0), new Vector3f(50,0,0),
            new ColorRGBA(1,0.2f,0.2f,0.8f), "AxisX");
        // Z axis line (blue)
        addAxisLine(new Vector3f(0,0,-50), new Vector3f(0,0,50),
            new ColorRGBA(0.2f,0.4f,1,0.8f), "AxisZ");
        // Y axis line (green)
        addAxisLine(new Vector3f(0,-50,0), new Vector3f(0,50,0),
            new ColorRGBA(0.2f,1,0.2f,0.8f), "AxisY");
    }

    private void addAxisLine(Vector3f from, Vector3f to, ColorRGBA col, String name) {
        com.jme3.scene.shape.Line line = new com.jme3.scene.shape.Line(from, to);
        Geometry geo = new Geometry(name, line);
        Material mat = new Material(assetManager, "Common/MatDefs/Misc/Unshaded.j3md");
        mat.setColor("Color", col);
        mat.getAdditionalRenderState().setLineWidth(1.5f);
        geo.setMaterial(mat);
        mGizmoNode.attachChild(geo);
    }

    // ─────────────────────────────────────────────────────────
    private void setupEnvironmentProbe() {
        // IBL environment probe for PBR materials
        EnvironmentCamera envCam = new EnvironmentCamera(128, Vector3f.ZERO);
        stateManager.attach(envCam);

        mEnvProbe = LightProbeFactory.makeProbe(
            stateManager.getState(EnvironmentCamera.class),
            rootNode,
            new JobProgressAdapter<LightProbe>() {
                @Override
                public void done(LightProbe result) {
                    enqueue(() -> {
                        rootNode.addLight(result);
                        mBridge.log("JME", "Environment probe ready");
                        return null;
                    });
                }
            });
    }

    // ═══════════════════════════════════════════════════════
    //  Update loop
    // ═══════════════════════════════════════════════════════
    @Override
    public void simpleUpdate(float tpf) {
        if (mEditorMode) {
            mGizmoNode.setCullHint(mShowGrid
                ? Spatial.CullHint.Never : Spatial.CullHint.Always);
        } else {
            mGizmoNode.setCullHint(Spatial.CullHint.Always);
        }
        // Sync physics transforms back to scene nodes (game mode)
        if (!mEditorMode) {
            mBridge.mPhysics.getRigidBodies().forEach((physId, rbc) -> {
                // Physics → visual sync happens automatically via RigidBodyControl
            });
        }
    }

    @Override
    public void simpleRender(RenderManager rm) {}

    // ═══════════════════════════════════════════════════════
    //  Post-processing toggles (called from C++ via JNI)
    // ═══════════════════════════════════════════════════════
    public void setBloomEnabled(boolean on) {
        enqueue(() -> { mBloom.setEnabled(on); return null; });
    }
    public void setBloomIntensity(float v) {
        enqueue(() -> { mBloom.setBloomIntensity(v); return null; });
    }
    public void setSSAOEnabled(boolean on) {
        enqueue(() -> { mSSAO.setEnabled(on); return null; });
    }
    public void setDOFEnabled(boolean on) {
        enqueue(() -> { mDOF.setEnabled(on); return null; });
    }
    public void setDOFFocusDistance(float d) {
        enqueue(() -> { mDOF.setFocusDistance(d); return null; });
    }
    public void setDOFBlurScale(float s) {
        enqueue(() -> { mDOF.setBlurScale(s); return null; });
    }
    public void setFXAAEnabled(boolean on) {
        enqueue(() -> { mFXAA.setEnabled(on); return null; });
    }
    public void setGridVisible(boolean on) {
        mShowGrid = on;
    }
    public void setShadowIntensity(float v) {
        enqueue(() -> { mShadowRenderer.setShadowIntensity(v); return null; });
    }
    public void setSunDirection(float x, float y, float z) {
        enqueue(() -> {
            mSunLight.setDirection(new Vector3f(x,y,z).normalizeLocal());
            return null;
        });
    }

    // ═══════════════════════════════════════════════════════
    //  Accessors
    // ═══════════════════════════════════════════════════════
    public void          setEditorMode(boolean editor) { mEditorMode = editor; }
    public void          setIs3D(boolean is3D)         { mIs3D = is3D; }
    public boolean       isEditorMode()                { return mEditorMode; }
    public ForgeCommandStack getCommandStack()         { return mCmdStack; }
    public BulletAppState    getBulletAppState()       { return mBulletState; }
    public PhysicsManager    getPhysicsManager()       { return mBridge.mPhysics; }
}
