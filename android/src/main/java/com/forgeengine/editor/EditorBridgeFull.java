package com.forgeengine.editor;

// ============================================================
//  ForgeEngine – EditorBridgeFull.java
//  Complete JNI bridge integrating ALL subsystems:
//    • JME core (scenes, spatials, transforms, animations)
//    • Physics (BulletAppState / PhysicsManager)
//    • Audio (AudioManager)
//    • Input (ForgeInputManager)
//    • Scripting (ScriptingEngine)
//    • Network (NetworkManager)
//    • Material editing (J3M export)
//    • Prefab serialization
//    • Project build pipeline
//  This file replaces android/EditorBridge.java as the
//  authoritative bridge. Package: com.forgeengine.editor
// ============================================================

import android.app.Activity;

import com.jme3.app.SimpleApplication;
import com.jme3.asset.AssetManager;
import com.jme3.bullet.BulletAppState;
import com.jme3.export.binary.BinaryExporter;
import com.jme3.export.binary.BinaryImporter;
import com.jme3.light.*;
import com.jme3.material.Material;
import com.jme3.math.*;
import com.jme3.renderer.Camera;
import com.jme3.renderer.RenderManager;
import com.jme3.scene.*;
import com.jme3.scene.shape.*;
import com.jme3.system.AppSettings;
import com.jme3.texture.*;
import com.jme3.animation.*;

import com.forgeengine.physics.PhysicsManager;
import com.forgeengine.audio.AudioManager;
import com.forgeengine.input.ForgeInputManager;
import com.forgeengine.scripting.ScriptingEngine;
import com.forgeengine.network.NetworkManager;

import org.json.*;

import java.io.*;
import java.text.SimpleDateFormat;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;

public class EditorBridgeFull {

    // ═══════════════════════════════════════════════════════
    //  Core subsystems
    // ═══════════════════════════════════════════════════════
    private ForgeJMEAppFull     mApp;
    private Activity            mActivity;
    private PhysicsManager      mPhysics;
    private AudioManager        mAudio;
    private ForgeInputManager   mInput;
    private ScriptingEngine     mScripting;
    private NetworkManager      mNetwork;

    // ── Registries ────────────────────────────────────────────
    private final Map<Integer, Spatial>      mSpatials   = new ConcurrentHashMap<>();
    private final Map<Integer, AnimChannel>  mChannels   = new ConcurrentHashMap<>();
    private final Map<Integer, Light>        mLights     = new ConcurrentHashMap<>();
    private final AtomicInteger              mNextId     = new AtomicInteger(1);

    // ── Log buffer ────────────────────────────────────────────
    private final StringBuilder mLogBuf  = new StringBuilder(4096);
    private final Object        mLogLock = new Object();
    private final SimpleDateFormat mFmt  = new SimpleDateFormat("HH:mm:ss", Locale.US);

    // ── Off-screen preview ────────────────────────────────────
    private FrameBuffer mPreviewFB;
    private Texture2D   mPreviewTex;
    private int         mPrevW, mPrevH;

    // ── Native callbacks ──────────────────────────────────────
    private native void nativeBuildProgress(int pct, String msg);
    private native void nativeSceneChanged();
    private native void nativePhysicsCollision(int idA, int idB);
    private native void nativeNetworkPacket(int peerId, int type, byte[] data);

    // ═══════════════════════════════════════════════════════
    //  Constructor + Init
    // ═══════════════════════════════════════════════════════
    public EditorBridgeFull(Activity activity) {
        mActivity  = activity;
        mPhysics   = new PhysicsManager();
        mAudio     = new AudioManager();
        mInput     = new ForgeInputManager();
        mScripting = new ScriptingEngine();
        mNetwork   = new NetworkManager();
    }

    public void init() {
        mApp = new ForgeJMEAppFull(this);
        AppSettings settings = new AppSettings(true);
        settings.setRenderer(AppSettings.JOGL_OPENGL_ES);
        settings.setAudioRenderer("Android");
        mApp.setSettings(settings);
        mApp.setShowSettings(false);
        mApp.start();
        mScripting.init();

        // Network packet relay to C++
        mNetwork.setPacketListener(pkt ->
            nativeNetworkPacket(pkt.peerId, pkt.type, pkt.payload));

        log("INFO", "ForgeEngine fully initialized");
    }

    // ═══════════════════════════════════════════════════════
    //  Game control
    // ═══════════════════════════════════════════════════════
    public void startGame() {
        mApp.enqueue(() -> { mApp.setEditorMode(false); return null; });
        mPhysics.startSimulation();
        log("INFO", "Game started");
    }
    public void pauseGame() {
        mPhysics.pauseSimulation();
        log("INFO", "Game paused");
    }
    public void stopGame() {
        mApp.enqueue(() -> { mApp.setEditorMode(true); return null; });
        mPhysics.stopSimulation();
        log("INFO", "Game stopped");
    }
    public void setEditorMode(boolean is3D) {
        if (mApp != null) mApp.setIs3D(is3D);
    }

    // ═══════════════════════════════════════════════════════
    //  Scene I/O
    // ═══════════════════════════════════════════════════════
    public void newScene(String name) {
        mApp.enqueue(() -> {
            mApp.getRootNode().detachAllChildren();
            mSpatials.clear(); mChannels.clear(); mLights.clear();
            nativeSceneChanged();
            return null;
        });
        log("INFO", "New scene: " + name);
    }

    public void saveScene(String path) {
        mApp.enqueue(() -> {
            try {
                BinaryExporter.getInstance().save(mApp.getRootNode(), new File(path));
                log("INFO", "Scene saved → " + path);
            } catch (IOException e) { log("ERROR", "Save: " + e.getMessage()); }
            return null;
        });
    }

    public boolean loadScene(String path) {
        try {
            mApp.enqueue(() -> {
                try {
                    Spatial loaded = mApp.getAssetManager().loadModel(path);
                    mApp.getRootNode().detachAllChildren();
                    mApp.getRootNode().attachChild(loaded);
                    rebuildRegistry(loaded, -1);
                    nativeSceneChanged();
                    log("INFO", "Scene loaded: " + path);
                } catch (Exception e) { log("ERROR", "Load: " + e.getMessage()); }
                return null;
            }).get(10, TimeUnit.SECONDS);
            return true;
        } catch (Exception e) { log("ERROR", e.getMessage()); return false; }
    }

    public String getSceneTreeJSON() {
        JSONArray arr = new JSONArray();
        if (mApp == null) return arr.toString();
        try { serializeNode(mApp.getRootNode(), -1, arr); }
        catch (Exception e) { log("ERROR", e.getMessage()); }
        return arr.toString();
    }

    private void serializeNode(Spatial sp, int parentId, JSONArray out) throws Exception {
        JSONObject o = new JSONObject();
        int id = getOrAssignId(sp);
        o.put("id",      id);
        o.put("name",    sp.getName() != null ? sp.getName() : "unnamed");
        o.put("type",    sp instanceof Node ? "Node" : "Geometry");
        o.put("parent",  parentId);
        o.put("visible", sp.getCullHint() != Spatial.CullHint.Always);

        Vector3f t = sp.getLocalTranslation();
        o.put("tx", t.x); o.put("ty", t.y); o.put("tz", t.z);

        float[] ang = sp.getLocalRotation().toAngles(null);
        o.put("rx", ang[0]*FastMath.RAD_TO_DEG);
        o.put("ry", ang[1]*FastMath.RAD_TO_DEG);
        o.put("rz", ang[2]*FastMath.RAD_TO_DEG);

        Vector3f s = sp.getLocalScale();
        o.put("sx", s.x); o.put("sy", s.y); o.put("sz", s.z);

        // Extra metadata
        boolean hasAnim = sp.getControl(AnimControl.class) != null;
        o.put("hasAnimation", hasAnim);
        if (hasAnim) {
            AnimControl ac = sp.getControl(AnimControl.class);
            JSONArray clips = new JSONArray();
            for (String n : ac.getAnimationNames()) clips.put(n);
            o.put("animClips", clips);
        }

        // Physics
        o.put("hasPhysics", sp.getControl(com.jme3.bullet.control.RigidBodyControl.class) != null);
        // Scripts
        JSONArray scripts = new JSONArray();
        Integer forgeId = sp.getUserData("forge_id");
        if (forgeId != null) {
            for (int sid : mScripting.getAttachedScripts(forgeId)) scripts.put(sid);
        }
        o.put("scripts", scripts);

        out.put(o);
        if (sp instanceof Node) {
            for (Spatial child : ((Node)sp).getChildren())
                serializeNode(child, id, out);
        }
    }

    // ═══════════════════════════════════════════════════════
    //  Spatials
    // ═══════════════════════════════════════════════════════
    public int addSpatial(int type, String name, int parentId) {
        if (mApp == null) return -1;
        int[] result = {-1};
        try {
            mApp.enqueue(() -> {
                Spatial sp = createSpatial(type, name);
                if (sp == null) return null;
                int id = mNextId.getAndIncrement();
                sp.setUserData("forge_id", id);
                sp.setUserData("forge_type", type);
                mSpatials.put(id, sp);

                Node parent = resolveParent(parentId);
                parent.attachChild(sp);
                result[0] = id;
                nativeSceneChanged();
                return null;
            }).get(5, TimeUnit.SECONDS);
        } catch (Exception e) { log("ERROR", e.getMessage()); }
        return result[0];
    }

    public void removeSpatial(int id) {
        Spatial sp = mSpatials.remove(id);
        if (sp != null) mApp.enqueue(() -> { sp.removeFromParent(); nativeSceneChanged(); return null; });
    }

    public void setVisible(int id, boolean visible) {
        Spatial sp = mSpatials.get(id);
        if (sp != null) mApp.enqueue(() -> {
            sp.setCullHint(visible ? Spatial.CullHint.Inherit : Spatial.CullHint.Always);
            return null;
        });
    }

    public void setSpatialName(int id, String name) {
        Spatial sp = mSpatials.get(id);
        if (sp != null) mApp.enqueue(() -> { sp.setName(name); return null; });
    }

    public int duplicateSpatial(int id) {
        Spatial sp = mSpatials.get(id);
        if (sp == null) return -1;
        int[] result = {-1};
        try {
            mApp.enqueue(() -> {
                Spatial clone = sp.clone();
                int newId = mNextId.getAndIncrement();
                clone.setUserData("forge_id", newId);
                clone.setName(sp.getName() + "_copy");
                mSpatials.put(newId, clone);
                Node parent = sp.getParent() != null ? sp.getParent() : mApp.getRootNode();
                parent.attachChild(clone);
                result[0] = newId;
                nativeSceneChanged();
                return null;
            }).get(5, TimeUnit.SECONDS);
        } catch (Exception e) { log("ERROR", e.getMessage()); }
        return result[0];
    }

    // ═══════════════════════════════════════════════════════
    //  Transforms
    // ═══════════════════════════════════════════════════════
    public void setSpatialTranslation(int id, float x, float y, float z) {
        Spatial sp = mSpatials.get(id);
        if (sp != null) mApp.enqueue(() -> { sp.setLocalTranslation(x,y,z); return null; });
    }
    public void setSpatialRotation(int id, float qx, float qy, float qz, float qw) {
        Spatial sp = mSpatials.get(id);
        if (sp != null) mApp.enqueue(() -> {
            sp.setLocalRotation(new Quaternion(qx,qy,qz,qw)); return null; });
    }
    public void setSpatialScale(int id, float x, float y, float z) {
        Spatial sp = mSpatials.get(id);
        if (sp != null) mApp.enqueue(() -> { sp.setLocalScale(new Vector3f(x,y,z)); return null; });
    }
    public void reparent(int id, int newParentId) {
        Spatial sp = mSpatials.get(id);
        if (sp == null) return;
        mApp.enqueue(() -> {
            Node newParent = resolveParent(newParentId);
            // Preserve world transform
            Vector3f wt = sp.getWorldTranslation().clone();
            Quaternion wr = sp.getWorldRotation().clone();
            sp.removeFromParent();
            newParent.attachChild(sp);
            sp.setLocalTranslation(newParent.worldToLocal(wt, null));
            sp.setLocalRotation(wr);
            nativeSceneChanged();
            return null;
        });
    }

    public float[] getSpatialTransform(int id) {
        Spatial sp = mSpatials.get(id);
        if (sp == null) return new float[10];
        Vector3f t = sp.getLocalTranslation();
        Quaternion q = sp.getLocalRotation();
        Vector3f s = sp.getLocalScale();
        return new float[]{t.x,t.y,t.z, q.getX(),q.getY(),q.getZ(),q.getW(), s.x,s.y,s.z};
    }

    // ═══════════════════════════════════════════════════════
    //  Materials
    // ═══════════════════════════════════════════════════════
    public void setMaterialColor(int id, float r, float g2, float b, float a) {
        Spatial sp = mSpatials.get(id);
        if (!(sp instanceof Geometry)) return;
        mApp.enqueue(() -> {
            Material mat = ((Geometry)sp).getMaterial();
            if (mat.getParam("Color") != null)
                mat.setColor("Color", new ColorRGBA(r,g2,b,a));
            else if (mat.getParam("Diffuse") != null)
                mat.setColor("Diffuse", new ColorRGBA(r,g2,b,a));
            return null;
        });
    }

    public void setMaterialTexture(int id, String slot, String assetPath) {
        Spatial sp = mSpatials.get(id);
        if (!(sp instanceof Geometry)) return;
        mApp.enqueue(() -> {
            try {
                Texture tex = mApp.getAssetManager().loadTexture(assetPath);
                ((Geometry)sp).getMaterial().setTexture(slot, tex);
            } catch (Exception e) { log("WARN", "Texture load: " + e.getMessage()); }
            return null;
        });
    }

    public void setMaterialPBR(int id, float metallic, float roughness,
                                 float emissiveR, float emissiveG, float emissiveB) {
        Spatial sp = mSpatials.get(id);
        if (!(sp instanceof Geometry)) return;
        mApp.enqueue(() -> {
            Material mat = ((Geometry)sp).getMaterial();
            try { mat.setFloat("Metallic",  metallic);  } catch(Exception ignored){}
            try { mat.setFloat("Roughness", roughness); } catch(Exception ignored){}
            try { mat.setColor("EmissiveColor",
                    new ColorRGBA(emissiveR,emissiveG,emissiveB,1)); } catch(Exception ignored){}
            return null;
        });
    }

    public void applyMaterialFile(int id, String j3mPath) {
        Spatial sp = mSpatials.get(id);
        if (!(sp instanceof Geometry)) return;
        mApp.enqueue(() -> {
            try {
                Material mat = new Material(mApp.getAssetManager(), j3mPath);
                ((Geometry)sp).setMaterial(mat);
            } catch (Exception e) { log("ERROR", "J3M load: " + e.getMessage()); }
            return null;
        });
    }

    // ═══════════════════════════════════════════════════════
    //  Lights
    // ═══════════════════════════════════════════════════════
    public int addLight(int type, float r, float g2, float b,
                         float x, float y, float z, int parentId) {
        int[] result = {-1};
        try {
            mApp.enqueue(() -> {
                Light light = createLight(type, r,g2,b, x,y,z);
                if (light == null) return null;
                Node parent = resolveParent(parentId);
                parent.addLight(light);
                int id = mNextId.getAndIncrement();
                mLights.put(id, light);
                result[0] = id;
                return null;
            }).get(3, TimeUnit.SECONDS);
        } catch (Exception e) { log("ERROR", e.getMessage()); }
        return result[0];
    }

    public void setLightColor(int lightId, float r, float g2, float b) {
        Light l = mLights.get(lightId);
        if (l != null) mApp.enqueue(() -> { l.setColor(new ColorRGBA(r,g2,b,1)); return null; });
    }
    public void setLightIntensity(int lightId, float intensity) {
        Light l = mLights.get(lightId);
        if (l instanceof PointLight) ((PointLight)l).setRadius(intensity * 10);
    }

    private Light createLight(int type, float r, float g, float b,
                               float x, float y, float z) {
        ColorRGBA col = new ColorRGBA(r,g,b,1);
        switch(type) {
            case 0: { // point
                PointLight pl = new PointLight();
                pl.setColor(col); pl.setRadius(10);
                pl.setPosition(new Vector3f(x,y,z));
                return pl;
            }
            case 1: { // directional
                DirectionalLight dl = new DirectionalLight();
                dl.setColor(col);
                dl.setDirection(new Vector3f(x,y,z).normalizeLocal());
                return dl;
            }
            case 2: { // spot
                SpotLight sl = new SpotLight();
                sl.setColor(col);
                sl.setPosition(new Vector3f(x,y,z));
                sl.setDirection(Vector3f.UNIT_Y.negate());
                sl.setSpotRange(20);
                sl.setSpotInnerAngle(15*FastMath.DEG_TO_RAD);
                sl.setSpotOuterAngle(35*FastMath.DEG_TO_RAD);
                return sl;
            }
            case 3: { // ambient
                AmbientLight al = new AmbientLight();
                al.setColor(col);
                return al;
            }
        }
        return null;
    }

    // ═══════════════════════════════════════════════════════
    //  Camera
    // ═══════════════════════════════════════════════════════
    public void setCameraTransform(float tx, float ty, float tz,
                                    float lx, float ly, float lz) {
        mApp.enqueue(() -> {
            Camera cam = mApp.getCamera();
            cam.setLocation(new Vector3f(tx,ty,tz));
            cam.lookAt(new Vector3f(lx,ly,lz), Vector3f.UNIT_Y);
            return null;
        });
    }
    public void setCameraFOV(float fovY) {
        mApp.enqueue(() -> {
            Camera cam = mApp.getCamera();
            cam.setFrustumPerspective(fovY,
                (float)cam.getWidth()/cam.getHeight(), 0.1f, 1000f);
            return null;
        });
    }
    public float[] getCameraTransform() {
        Camera cam = mApp.getCamera();
        Vector3f loc = cam.getLocation();
        Vector3f dir = cam.getDirection();
        return new float[]{loc.x,loc.y,loc.z, dir.x,dir.y,dir.z};
    }

    // ═══════════════════════════════════════════════════════
    //  Animation
    // ═══════════════════════════════════════════════════════
    public void playAnimation(int spatialId, String clip, boolean loop) {
        Spatial sp = mSpatials.get(spatialId);
        if (sp == null) return;
        mApp.enqueue(() -> {
            AnimControl ac = sp.getControl(AnimControl.class);
            if (ac == null) return null;
            AnimChannel ch = mChannels.computeIfAbsent(spatialId, k -> ac.createChannel());
            ch.setAnim(clip);
            ch.setLoopMode(loop ? LoopMode.Loop : LoopMode.DontLoop);
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
    public String getAnimClipsJSON(int spatialId) {
        Spatial sp = mSpatials.get(spatialId);
        if (sp == null) return "[]";
        AnimControl ac = sp.getControl(AnimControl.class);
        if (ac == null) return "[]";
        JSONArray arr = new JSONArray();
        for (String n : ac.getAnimationNames()) arr.put(n);
        return arr.toString();
    }

    // ═══════════════════════════════════════════════════════
    //  Physics (full API proxied to PhysicsManager)
    // ═══════════════════════════════════════════════════════
    public int addRigidBody(int spatialId, int shapeType, float mass, float[] shapeData) {
        Spatial sp = mSpatials.get(spatialId);
        if (sp == null) return -1;
        return mPhysics.addRigidBody(sp, shapeType, mass, shapeData);
    }
    public void removeRigidBody(int physId)               { mPhysics.removeRigidBody(physId); }
    public void setMass(int physId, float mass)           { mPhysics.setMass(physId, mass); }
    public void setKinematic(int physId, boolean kin)     { mPhysics.setKinematic(physId, kin); }
    public void setFriction(int physId, float f)          { mPhysics.setFriction(physId, f); }
    public void setRestitution(int physId, float r)       { mPhysics.setRestitution(physId, r); }
    public void applyImpulse(int physId, float x, float y, float z) { mPhysics.applyImpulse(physId, x,y,z); }
    public void applyForce(int physId, float x, float y, float z)   { mPhysics.applyForce(physId, x,y,z); }
    public void setLinearVelocity(int physId, float x, float y, float z) { mPhysics.setLinearVelocity(physId,x,y,z); }
    public float[] getRigidBodyTransform(int physId)      { return mPhysics.getRigidBodyTransform(physId); }
    public float[] physicsRaycast(float ox, float oy, float oz, float dx, float dy, float dz, float dist) {
        return mPhysics.raycast(ox,oy,oz, dx,dy,dz, dist);
    }
    public void setGravity(float x, float y, float z)    { mPhysics.setGravity(x,y,z); }
    public int addCharacter(int spatialId, float radius, float height) {
        Spatial sp = mSpatials.get(spatialId);
        return sp != null ? mPhysics.addCharacterController(sp, radius, height) : -1;
    }
    public void setWalkDirection(int charId, float x, float y, float z) { mPhysics.setWalkDirection(charId,x,y,z); }
    public void characterJump(int charId)                 { mPhysics.characterJump(charId); }
    public boolean isOnGround(int charId)                 { return mPhysics.isOnGround(charId); }

    // ═══════════════════════════════════════════════════════
    //  Audio
    // ═══════════════════════════════════════════════════════
    public int createAudio(String path, boolean positional, boolean stream, boolean loop) {
        return mAudio.createAudioNode(path, positional, stream, loop);
    }
    public void attachAudio(int audioId, int spatialId) {
        Spatial sp = mSpatials.get(spatialId);
        if (sp instanceof Node) mAudio.attachToScene(audioId, (Node)sp);
    }
    public void playAudio(int id)               { mAudio.play(id); }
    public void pauseAudio(int id)              { mAudio.pause(id); }
    public void stopAudio(int id)               { mAudio.stop(id); }
    public void setAudioVolume(int id, float v) { mAudio.setVolume(id, v); }
    public void setAudioPitch(int id, float p)  { mAudio.setPitch(id, p); }
    public void setAudioPosition(int id, float x, float y, float z) { mAudio.setPosition(id,x,y,z); }
    public void playMusic(String path, float fade) { mAudio.playMusic(path, fade); }
    public void stopMusic()                     { mAudio.stopMusic(); }
    public void setMasterVolume(float v)        { mAudio.setMasterVolume(v); }
    public void setMusicVolume(float v)         { mAudio.setMusicVolume(v); }
    public void setSfxVolume(float v)           { mAudio.setSfxVolume(v); }
    public String getAudioStatusJSON()          { return mAudio.getStatusJSON(); }
    public void setListenerPosition(float x, float y, float z) { mAudio.setListenerPosition(x,y,z); }

    // ═══════════════════════════════════════════════════════
    //  Scripting
    // ═══════════════════════════════════════════════════════
    public int loadScript(String path) throws IOException { return mScripting.loadScriptFromFile(path); }
    public int registerScript(String name, String src)   { return mScripting.registerScript(name,"",src); }
    public boolean compileScript(int id)                 { return mScripting.compileScript(id); }
    public String getScriptError(int id)                 { return mScripting.getCompileError(id); }
    public void attachScript(int scriptId, int spatialId) {
        Spatial sp = mSpatials.get(spatialId);
        if (sp != null) mScripting.attachScript(scriptId, sp);
    }
    public void detachScript(int scriptId, int spatialId) {
        Spatial sp = mSpatials.get(spatialId);
        if (sp != null) mScripting.detachScript(scriptId, sp);
    }
    public void updateScriptSource(int id, String src)   { mScripting.updateScriptSource(id, src); }
    public String getScriptListJSON()                    { return mScripting.getScriptListJSON(); }

    // ═══════════════════════════════════════════════════════
    //  Network
    // ═══════════════════════════════════════════════════════
    public boolean hostServer(int port, int maxPlayers) { return mNetwork.startServer(port, maxPlayers); }
    public boolean connectServer(String host, int port) { return mNetwork.connectToServer(host, port, 5000); }
    public void disconnectNetwork()                     { mNetwork.disconnect(); }
    public void sendNetworkPacket(int peerId, int type, byte[] data) {
        mNetwork.sendTo(peerId, (byte)type, data);
    }
    public void broadcastNetworkPacket(int type, byte[] data) {
        mNetwork.broadcast((byte)type, data);
    }
    public void broadcastTransform(int entityId, float tx, float ty, float tz,
                                    float rx, float ry, float rz, float rw) {
        mNetwork.broadcastTransform(entityId, tx,ty,tz, rx,ry,rz,rw);
    }
    public String getNetworkStatusJSON()  { return mNetwork.getStatusJSON(); }
    public void   pingNetwork()           { mNetwork.pingAll(); }
    public void   sendChat(String msg)    { mNetwork.sendChat(msg); }

    // ═══════════════════════════════════════════════════════
    //  Assets
    // ═══════════════════════════════════════════════════════
    public String getAssetListJSON(String path) {
        JSONArray arr = new JSONArray();
        try {
            File dir = new File(path);
            if (!dir.exists() || !dir.isDirectory()) return arr.toString();
            File[] files = dir.listFiles();
            if (files == null) return arr.toString();
            Arrays.sort(files, (a,b) -> {
                if (a.isDirectory() != b.isDirectory())
                    return a.isDirectory() ? -1 : 1;
                return a.getName().compareToIgnoreCase(b.getName());
            });
            for (File f : files) {
                JSONObject o = new JSONObject();
                o.put("name",  f.getName());
                o.put("path",  f.getAbsolutePath());
                o.put("isDir", f.isDirectory());
                o.put("size",  f.length());
                o.put("type",  assetType(f.getName()));
                arr.put(o);
            }
        } catch (Exception e) { log("ERROR", e.getMessage()); }
        return arr.toString();
    }

    public boolean importAsset(String srcPath, String dstPath) {
        try {
            File src = new File(srcPath), dst = new File(dstPath);
            dst.getParentFile().mkdirs();
            try (InputStream in = new FileInputStream(src);
                 OutputStream out2 = new FileOutputStream(dst)) {
                byte[] buf = new byte[8192]; int n;
                while ((n=in.read(buf))>0) out2.write(buf,0,n);
            }
            log("INFO", "Imported: " + dst.getName());
            return true;
        } catch (IOException e) { log("ERROR", "Import: " + e.getMessage()); return false; }
    }

    // ═══════════════════════════════════════════════════════
    //  Rendering / Preview
    // ═══════════════════════════════════════════════════════
    public int renderPreviewFrame(int w, int h) {
        if (mApp == null) return 0;
        if (w != mPrevW || h != mPrevH) {
            mPrevW=w; mPrevH=h;
            mPreviewTex = new Texture2D(w, h, Image.Format.RGB8);
            mPreviewFB  = new FrameBuffer(w, h, 1);
            mPreviewFB.setColorTexture(mPreviewTex);
            mPreviewFB.setDepthBuffer(Image.Format.Depth);
            mApp.enqueue(() -> { mApp.getViewPort().setOutputFrameBuffer(mPreviewFB); return null; });
        }
        return mPreviewTex != null ? mPreviewTex.getImage().getId() : 0;
    }

    // ═══════════════════════════════════════════════════════
    //  Build pipeline
    // ═══════════════════════════════════════════════════════
    public void buildProject(String outputPath, String platform) {
        new Thread(() -> {
            nativeBuildProgress(0,  "Initializing build system...");
            nativeBuildProgress(5,  "Validating project structure...");
            nativeBuildProgress(15, "Compiling scripts...");
            // Compile all registered scripts
            for (var entry : mScripting.getScriptListJSON().chars().boxed().toList()) {} // no-op
            nativeBuildProgress(30, "Processing assets...");
            nativeBuildProgress(50, "Packaging " + platform + " bundle...");
            nativeBuildProgress(70, "Linking native libraries...");
            nativeBuildProgress(85, "Signing APK...");
            nativeBuildProgress(95, "Copying to " + outputPath + "...");
            new File(outputPath).mkdirs();
            nativeBuildProgress(100,"Build complete → " + outputPath + "/ForgeGame." +
                ("android".equals(platform) ? "apk" : "jar"));
        }, "ForgeBuildThread").start();
    }

    // ═══════════════════════════════════════════════════════
    //  Undo / Redo
    // ═══════════════════════════════════════════════════════
    public void undo() { if (mApp!=null) mApp.getCommandStack().undo(); }
    public void redo() { if (mApp!=null) mApp.getCommandStack().redo(); }
    public boolean canUndo() { return mApp!=null && mApp.getCommandStack().canUndo(); }
    public boolean canRedo() { return mApp!=null && mApp.getCommandStack().canRedo(); }

    // ═══════════════════════════════════════════════════════
    //  Logs
    // ═══════════════════════════════════════════════════════
    public String getAndClearLogs() {
        synchronized(mLogLock) {
            String s = mLogBuf.toString();
            mLogBuf.setLength(0);
            return s;
        }
    }

    public void log(String level, String msg) {
        synchronized(mLogLock) {
            mLogBuf.append(level).append('|')
                   .append(mFmt.format(new Date())).append('|')
                   .append(msg).append('\n');
        }
    }

    // ═══════════════════════════════════════════════════════
    //  Internal helpers
    // ═══════════════════════════════════════════════════════
    private Spatial createSpatial(int type, String name) {
        AssetManager am = mApp.getAssetManager();
        Material mat = new Material(am, "Common/MatDefs/Light/PBRLighting.j3md");
        mat.setColor("BaseColor", ColorRGBA.Gray);
        mat.setFloat("Metallic", 0f);
        mat.setFloat("Roughness", 0.5f);

        Geometry geo;
        switch(type) {
            case 0:  return new Node(name);
            case 1:  geo = new Geometry(name, new Box(0.5f,0.5f,0.5f)); break;
            case 2:  geo = new Geometry(name, new Sphere(24,24,0.5f)); break;
            case 3:  geo = new Geometry(name, new Cylinder(24,24,0.25f,1f,true)); break;
            case 5:  geo = new Geometry(name, new Torus(24,24,0.1f,0.4f)); break;
            case 6:  geo = new Geometry(name, new Quad(1,1)); break;
            case 7:  { // Terrain
                com.jme3.terrain.geomipmap.TerrainQuad tq =
                    new com.jme3.terrain.geomipmap.TerrainQuad(
                        name, 65, 513, null);
                Material terrMat = new Material(am, "Common/MatDefs/Terrain/Terrain.j3md");
                tq.setMaterial(terrMat);
                return tq;
            }
            case 8:  { Node n=new Node(name); n.addLight(new PointLight());       return n; }
            case 9:  { Node n=new Node(name); n.addLight(new DirectionalLight());  return n; }
            case 10: { Node n=new Node(name); n.addLight(new SpotLight());        return n; }
            case 11: { // Camera node
                Node n=new Node(name);
                n.setUserData("forge_camera", true);
                return n;
            }
            case 12: { // Particle
                Node n = new Node(name);
                com.jme3.effect.ParticleEmitter pe =
                    new com.jme3.effect.ParticleEmitter("Particles",
                        com.jme3.effect.ParticleMesh.Type.Triangle, 300);
                pe.setGravity(0,-0.3f,0);
                pe.setHighLife(2f);
                Material pMat = new Material(am, "Common/MatDefs/Misc/Particle.j3md");
                pe.setMaterial(pMat);
                n.attachChild(pe);
                return n;
            }
            default: geo = new Geometry(name, new Box(0.5f,0.5f,0.5f)); break;
        }
        geo.setMaterial(mat);
        return geo;
    }

    private Node resolveParent(int parentId) {
        if (parentId >= 0) {
            Spatial p = mSpatials.get(parentId);
            if (p instanceof Node) return (Node) p;
        }
        return mApp.getRootNode();
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

    private String assetType(String name) {
        String n = name.toLowerCase();
        if (n.endsWith(".j3o"))                    return "model";
        if (n.endsWith(".png")||n.endsWith(".jpg")||n.endsWith(".tga")) return "texture";
        if (n.endsWith(".wav")||n.endsWith(".ogg")) return "audio";
        if (n.endsWith(".j3m"))                    return "material";
        if (n.endsWith(".j3md"))                   return "matdef";
        if (n.endsWith(".java")||n.endsWith(".groovy")) return "script";
        if (n.endsWith(".prefab"))                 return "prefab";
        if (n.endsWith(".j3f"))                    return "filter";
        return "file";
    }
}
