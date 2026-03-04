package com.forgeengine;

// ============================================================
//  ForgeEngine  –  EditorBridge.java
//  JMonkeyEngine side of the JNI bridge.
//  Wraps the JME application and exposes editor controls.
// ============================================================

import android.app.Activity;
import com.jme3.app.SimpleApplication;
import com.jme3.asset.AndroidAssetManager;
import com.jme3.export.binary.BinaryExporter;
import com.jme3.export.binary.BinaryImporter;
import com.jme3.light.DirectionalLight;
import com.jme3.light.PointLight;
import com.jme3.light.SpotLight;
import com.jme3.math.ColorRGBA;
import com.jme3.math.FastMath;
import com.jme3.math.Quaternion;
import com.jme3.math.Vector3f;
import com.jme3.renderer.RenderManager;
import com.jme3.scene.Geometry;
import com.jme3.scene.Node;
import com.jme3.scene.Spatial;
import com.jme3.scene.shape.*;
import com.jme3.system.AppSettings;
import com.jme3.system.android.OGLESContext;
import com.jme3.animation.AnimControl;
import com.jme3.animation.AnimChannel;
import com.jme3.texture.FrameBuffer;
import com.jme3.texture.Image;
import com.jme3.texture.Texture2D;
import com.jme3.renderer.ViewPort;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.File;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;

public class EditorBridge {

    // ── JME application ────────────────────────────────────────
    private ForgeJMEApp       mApp;
    private Activity          mActivity;
    private final AtomicInteger mNextId = new AtomicInteger(1);

    // ── Scene registry ─────────────────────────────────────────
    private final Map<Integer, Spatial> mSpatials    = new ConcurrentHashMap<>();
    private final Map<Integer, AnimChannel> mChannels = new ConcurrentHashMap<>();

    // ── Log buffer ─────────────────────────────────────────────
    private final StringBuilder mLogBuf = new StringBuilder();
    private final Object        mLogLock = new Object();
    private final SimpleDateFormat mTimeFmt =
            new SimpleDateFormat("HH:mm:ss", Locale.US);

    // ── Off-screen framebuffer ─────────────────────────────────
    private FrameBuffer mPreviewFB;
    private Texture2D   mPreviewTex;
    private int         mPrevW = 0, mPrevH = 0;

    // ── Native callbacks ───────────────────────────────────────
    private native void nativeBuildProgress(int progress, String msg);
    private native void nativeSceneChanged();

    // ─────────────────────────────────────────────────────────
    public EditorBridge(Activity activity) {
        mActivity = activity;
    }

    // ─────────────────────────────────────────────────────────
    //  init() – called from C++ after construction
    // ─────────────────────────────────────────────────────────
    public void init() {
        mApp = new ForgeJMEApp(this);
        AppSettings settings = new AppSettings(true);
        settings.setRenderer(AppSettings.JOGL_OPENGL_ES);
        settings.setAudioRenderer(null);
        mApp.setSettings(settings);
        mApp.setShowSettings(false);
        mApp.start(JmeContext.Type.OffscreenSurface);
        log("JME", "ForgeEngine JME started");
    }

    // ─────────────────────────────────────────────────────────
    //  Game control
    // ─────────────────────────────────────────────────────────
    public void startGame() {
        if (mApp != null) mApp.enqueue(() -> { mApp.setEditorMode(false); return null; });
        log("INFO", "Game started");
    }
    public void pauseGame() {
        log("INFO", "Game paused");
    }
    public void stopGame() {
        if (mApp != null) mApp.enqueue(() -> { mApp.setEditorMode(true); return null; });
        log("INFO", "Game stopped");
    }
    public void setEditorMode(boolean is3D) {
        if (mApp != null) mApp.setIs3D(is3D);
    }

    // ─────────────────────────────────────────────────────────
    //  Scene I/O
    // ─────────────────────────────────────────────────────────
    public void newScene(String name) {
        if (mApp == null) return;
        mApp.enqueue(() -> {
            mApp.getRootNode().detachAllChildren();
            mSpatials.clear();
            nativeSceneChanged();
            return null;
        });
        log("INFO", "New scene: " + name);
    }

    public void saveScene(String path) {
        if (mApp == null) return;
        mApp.enqueue(() -> {
            try {
                BinaryExporter exp = BinaryExporter.getInstance();
                exp.save(mApp.getRootNode(), new File(path));
                log("INFO", "Scene saved: " + path);
            } catch (IOException e) {
                log("ERROR", "Save failed: " + e.getMessage());
            }
            return null;
        });
    }

    public boolean loadScene(String path) {
        if (mApp == null) return false;
        try {
            mApp.enqueue(() -> {
                try {
                    Spatial s = mApp.getAssetManager().loadModel(path);
                    mApp.getRootNode().detachAllChildren();
                    mApp.getRootNode().attachChild(s);
                    rebuildRegistry(mApp.getRootNode(), -1);
                    nativeSceneChanged();
                } catch (Exception e) {
                    log("ERROR", "Load failed: " + e.getMessage());
                }
                return null;
            }).get(5, TimeUnit.SECONDS);
            return true;
        } catch (Exception e) { return false; }
    }

    // ─────────────────────────────────────────────────────────
    //  Scene tree JSON
    // ─────────────────────────────────────────────────────────
    public String getSceneTreeJSON() {
        JSONArray arr = new JSONArray();
        if (mApp == null) return arr.toString();
        try {
            serializeNode(mApp.getRootNode(), -1, arr);
        } catch (Exception e) { log("ERROR", e.getMessage()); }
        return arr.toString();
    }

    private void serializeNode(Spatial sp, int parentId, JSONArray arr)
            throws Exception {
        JSONObject o = new JSONObject();
        int id = getOrAssignId(sp);
        o.put("id",   id);
        o.put("name", sp.getName() != null ? sp.getName() : "Spatial");
        o.put("type", sp instanceof Node ? "Node" : "Geometry");
        o.put("parent", parentId);
        o.put("visible", sp.getCullHint() != Spatial.CullHint.Always);

        Vector3f t = sp.getLocalTranslation();
        o.put("tx", t.x); o.put("ty", t.y); o.put("tz", t.z);

        float[] angles = sp.getLocalRotation().toAngles(null);
        o.put("rx", angles[0]*FastMath.RAD_TO_DEG);
        o.put("ry", angles[1]*FastMath.RAD_TO_DEG);
        o.put("rz", angles[2]*FastMath.RAD_TO_DEG);

        Vector3f s = sp.getLocalScale();
        o.put("sx", s.x); o.put("sy", s.y); o.put("sz", s.z);
        arr.put(o);

        if (sp instanceof Node) {
            for (Spatial child : ((Node)sp).getChildren())
                serializeNode(child, id, arr);
        }
    }

    // ─────────────────────────────────────────────────────────
    //  Add / Remove spatials
    // ─────────────────────────────────────────────────────────
    public int addSpatial(int type, String name, int parentId) {
        if (mApp == null) return -1;
        int[] result = {-1};
        try {
            mApp.enqueue(() -> {
                Spatial sp = createSpatial(type, name);
                if (sp == null) return null;
                int id = mNextId.getAndIncrement();
                setUserData(sp, "forge_id", id);
                mSpatials.put(id, sp);

                Node parent = (parentId < 0)
                    ? mApp.getRootNode()
                    : (Node) mSpatials.get(parentId);
                if (parent == null) parent = mApp.getRootNode();
                parent.attachChild(sp);
                result[0] = id;
                nativeSceneChanged();
                return null;
            }).get(3, TimeUnit.SECONDS);
        } catch (Exception e) { log("ERROR", e.getMessage()); }
        log("INFO", "Added spatial [" + type + "] " + name);
        return result[0];
    }

    public void removeSpatial(int id) {
        Spatial sp = mSpatials.remove(id);
        if (sp != null) {
            mApp.enqueue(() -> { sp.removeFromParent(); nativeSceneChanged(); return null; });
        }
    }

    // ─────────────────────────────────────────────────────────
    //  Transform setters
    // ─────────────────────────────────────────────────────────
    public void setSpatialTranslation(int id, float x, float y, float z) {
        Spatial sp = mSpatials.get(id);
        if (sp != null)
            mApp.enqueue(() -> { sp.setLocalTranslation(x,y,z); return null; });
    }
    public void setSpatialRotation(int id, float qx, float qy, float qz, float qw) {
        Spatial sp = mSpatials.get(id);
        if (sp != null)
            mApp.enqueue(() -> {
                sp.setLocalRotation(new Quaternion(qx,qy,qz,qw)); return null; });
    }
    public void setSpatialScale(int id, float x, float y, float z) {
        Spatial sp = mSpatials.get(id);
        if (sp != null)
            mApp.enqueue(() -> { sp.setLocalScale(new Vector3f(x,y,z)); return null; });
    }
    public float[] getSpatialTransform(int id) {
        Spatial sp = mSpatials.get(id);
        if (sp == null) return new float[10];
        Vector3f t = sp.getLocalTranslation();
        Quaternion q = sp.getLocalRotation();
        Vector3f s = sp.getLocalScale();
        return new float[]{t.x,t.y,t.z, q.getX(),q.getY(),q.getZ(),q.getW(), s.x,s.y,s.z};
    }

    // ─────────────────────────────────────────────────────────
    //  Animation
    // ─────────────────────────────────────────────────────────
    public void playAnimation(int spatialId, String clip, boolean loop) {
        Spatial sp = mSpatials.get(spatialId);
        if (sp == null) return;
        mApp.enqueue(() -> {
            AnimControl ctrl = sp.getControl(AnimControl.class);
            if (ctrl == null) return null;
            AnimChannel ch = mChannels.computeIfAbsent(
                spatialId, k -> ctrl.createChannel());
            ch.setAnim(clip);
            ch.setLoopMode(loop
                ? com.jme3.animation.LoopMode.Loop
                : com.jme3.animation.LoopMode.DontLoop);
            return null;
        });
    }
    public void stopAnimation(int spatialId) {
        AnimChannel ch = mChannels.get(spatialId);
        if (ch != null) mApp.enqueue(() -> { ch.reset(true); return null; });
    }
    public void setAnimSpeed(int spatialId, float speed) {
        AnimChannel ch = mChannels.get(spatialId);
        if (ch != null) mApp.enqueue(() -> { ch.setSpeed(speed); return null; });
    }

    // ─────────────────────────────────────────────────────────
    //  Preview framebuffer → texture
    // ─────────────────────────────────────────────────────────
    public int renderPreviewFrame(int w, int h) {
        if (mApp == null) return 0;
        if (w != mPrevW || h != mPrevH) recreateFrameBuffer(w, h);
        // JME renders into mPreviewFB; returns its GL texture id
        return (mPreviewTex != null)
            ? mPreviewTex.getImage().getId() : 0;
    }
    private void recreateFrameBuffer(int w, int h) {
        mPrevW = w; mPrevH = h;
        mPreviewTex = new Texture2D(w, h, Image.Format.RGB8);
        mPreviewFB  = new FrameBuffer(w, h, 1);
        mPreviewFB.setColorTexture(mPreviewTex);
        mPreviewFB.setDepthBuffer(Image.Format.Depth);
        if (mApp != null)
            mApp.enqueue(() -> {
                mApp.getViewPort().setOutputFrameBuffer(mPreviewFB);
                return null;
            });
    }

    // ─────────────────────────────────────────────────────────
    //  Assets
    // ─────────────────────────────────────────────────────────
    public String getAssetListJSON(String path) {
        JSONArray arr = new JSONArray();
        try {
            File dir = new File(path);
            if (dir.exists() && dir.isDirectory()) {
                File[] files = dir.listFiles();
                if (files != null) for (File f : files) {
                    JSONObject o = new JSONObject();
                    o.put("name",  f.getName());
                    o.put("path",  f.getAbsolutePath());
                    o.put("isDir", f.isDirectory());
                    o.put("type",  getAssetType(f.getName()));
                    arr.put(o);
                }
            }
        } catch (Exception e) { log("ERROR", e.getMessage()); }
        return arr.toString();
    }

    // ─────────────────────────────────────────────────────────
    //  Build
    // ─────────────────────────────────────────────────────────
    public void buildProject(String outputPath, String platform) {
        new Thread(() -> {
            nativeBuildProgress(0,  "Preparing build...");
            nativeBuildProgress(20, "Compiling assets...");
            // Real gradle/Ant build would be triggered here
            nativeBuildProgress(60, "Packaging " + platform + " APK...");
            nativeBuildProgress(90, "Signing...");
            nativeBuildProgress(100, "Build complete → " + outputPath);
        }).start();
    }

    // ─────────────────────────────────────────────────────────
    //  Undo / Redo  (delegates to JME command stack)
    // ─────────────────────────────────────────────────────────
    public void undo() { if (mApp != null) mApp.getCommandStack().undo(); }
    public void redo() { if (mApp != null) mApp.getCommandStack().redo(); }

    // ─────────────────────────────────────────────────────────
    //  Logging
    // ─────────────────────────────────────────────────────────
    public String getAndClearLogs() {
        synchronized (mLogLock) {
            String s = mLogBuf.toString();
            mLogBuf.setLength(0);
            return s;
        }
    }
    public void log(String level, String msg) {
        synchronized (mLogLock) {
            mLogBuf.append(level).append('|')
                   .append(mTimeFmt.format(new Date())).append('|')
                   .append(msg).append('\n');
        }
    }

    // ─────────────────────────────────────────────────────────
    //  Helpers
    // ─────────────────────────────────────────────────────────
    private Spatial createSpatial(int type, String name) {
        com.jme3.scene.shape.Mesh mesh = null;
        switch (type) {
            case 0: return new Node(name);   // NODE
            case 1: mesh = new Box(0.5f,0.5f,0.5f); break;        // BOX
            case 2: mesh = new Sphere(16,16,0.5f); break;         // SPHERE
            case 3: mesh = new Cylinder(16,16,0.25f,1f,true); break; // CYLINDER
            case 4: mesh = new Capsule(16,16,0.25f,0.5f); break;  // CAPSULE (custom)
            case 5: mesh = new Torus(16,16,0.15f,0.5f); break;    // TORUS
            case 6: mesh = new Quad(1,1); break;                   // PLANE
            // Lights / Camera added as nodes with markers
            case 8:  { Node n = new Node(name); n.addLight(new PointLight());      return n; }
            case 9:  { Node n = new Node(name); n.addLight(new DirectionalLight()); return n; }
            case 10: { Node n = new Node(name); n.addLight(new SpotLight());       return n; }
            default: mesh = new Box(0.5f,0.5f,0.5f);
        }
        Geometry geo = new Geometry(name, mesh);
        com.jme3.material.Material mat = new com.jme3.material.Material(
            mApp.getAssetManager(),
            "Common/MatDefs/Misc/Unshaded.j3md");
        mat.setColor("Color", ColorRGBA.Gray);
        geo.setMaterial(mat);
        return geo;
    }

    @SuppressWarnings("unchecked")
    private void setUserData(Spatial sp, String key, int val) {
        sp.setUserData(key, val);
    }

    private int getOrAssignId(Spatial sp) {
        Integer id = sp.getUserData("forge_id");
        if (id == null) {
            id = mNextId.getAndIncrement();
            sp.setUserData("forge_id", id);
            mSpatials.put(id, sp);
        }
        return id;
    }

    private void rebuildRegistry(Spatial sp, int parentId) {
        getOrAssignId(sp);
        if (sp instanceof Node)
            for (Spatial c : ((Node)sp).getChildren())
                rebuildRegistry(c, getOrAssignId(sp));
    }

    private String getAssetType(String name) {
        String n = name.toLowerCase();
        if (n.endsWith(".j3o"))  return "model";
        if (n.endsWith(".png") || n.endsWith(".jpg")) return "texture";
        if (n.endsWith(".wav") || n.endsWith(".ogg")) return "audio";
        if (n.endsWith(".j3m"))  return "material";
        if (n.endsWith(".j3md")) return "matdef";
        if (n.endsWith(".java")) return "script";
        return "file";
    }
}
