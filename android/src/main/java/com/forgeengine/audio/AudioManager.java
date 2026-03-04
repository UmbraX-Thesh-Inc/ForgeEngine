package com.forgeengine.audio;

// ============================================================
//  ForgeEngine – AudioManager.java
//  Full audio: 3D spatial sounds, music streaming, AudioNode
//  integration with JME AudioRenderer (OpenAL).
// ============================================================

import com.jme3.audio.*;
import com.jme3.asset.AssetManager;
import com.jme3.math.Vector3f;
import com.jme3.scene.Node;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;

public class AudioManager {

    private AssetManager          mAssets;
    private AudioRenderer         mRenderer;
    private Listener              mListener;
    private final Map<Integer, AudioNode> mNodes = new HashMap<>();
    private final AtomicInteger   mIdGen = new AtomicInteger(1);
    // Bus volumes
    private float mMasterVol = 1.f;
    private float mMusicVol  = 0.8f;
    private float mSfxVol    = 1.f;
    private int   mMusicId   = -1;

    // ── Init ──────────────────────────────────────────────────
    public void init(AssetManager assets, AudioRenderer renderer,
                     Listener listener) {
        mAssets   = assets;
        mRenderer = renderer;
        mListener = listener;
        // Default Doppler / distance model
        mRenderer.setEnvironment(new Environment(Environment.Garage));
    }

    // ─────────────────────────────────────────────────────────
    //  Listener (follows camera/player)
    // ─────────────────────────────────────────────────────────
    public void setListenerPosition(float x, float y, float z) {
        mListener.setLocation(new Vector3f(x,y,z));
    }
    public void setListenerOrientation(float dx, float dy, float dz,
                                        float ux, float uy, float uz) {
        mListener.setDirection(new Vector3f(dx,dy,dz));
        mListener.setUp(new Vector3f(ux,uy,uz));
    }

    // ─────────────────────────────────────────────────────────
    //  Create audio node
    // ─────────────────────────────────────────────────────────
    public int createAudioNode(String assetPath, boolean positional,
                                boolean stream, boolean loop) {
        AudioNode node = new AudioNode(mAssets, assetPath,
            stream ? AudioData.DataType.Stream
                   : AudioData.DataType.Buffer);
        node.setPositional(positional);
        node.setLooping(loop);
        node.setVolume(positional ? mSfxVol : mMusicVol);
        int id = mIdGen.getAndIncrement();
        mNodes.put(id, node);
        return id;
    }

    public void attachToScene(int id, Node parent) {
        AudioNode n = mNodes.get(id);
        if (n != null) parent.attachChild(n);
    }

    // ─────────────────────────────────────────────────────────
    //  Playback control
    // ─────────────────────────────────────────────────────────
    public void play(int id) {
        AudioNode n = mNodes.get(id);
        if (n != null) n.play();
    }
    public void playInstance(int id) {
        AudioNode n = mNodes.get(id);
        if (n != null) n.playInstance();
    }
    public void pause(int id) {
        AudioNode n = mNodes.get(id);
        if (n != null) n.pause();
    }
    public void stop(int id) {
        AudioNode n = mNodes.get(id);
        if (n != null) n.stop();
    }
    public void setVolume(int id, float vol) {
        AudioNode n = mNodes.get(id);
        if (n != null) n.setVolume(vol * mMasterVol);
    }
    public void setPitch(int id, float pitch) {
        AudioNode n = mNodes.get(id);
        if (n != null) n.setPitch(pitch);
    }
    public void setLooping(int id, boolean loop) {
        AudioNode n = mNodes.get(id);
        if (n != null) n.setLooping(loop);
    }

    // ─────────────────────────────────────────────────────────
    //  3D positioning
    // ─────────────────────────────────────────────────────────
    public void setPosition(int id, float x, float y, float z) {
        AudioNode n = mNodes.get(id);
        if (n != null) {
            n.setPositional(true);
            n.setLocalTranslation(x, y, z);
        }
    }
    public void setMaxDistance(int id, float dist) {
        AudioNode n = mNodes.get(id);
        if (n != null) n.setMaxDistance(dist);
    }
    public void setRefDistance(int id, float dist) {
        AudioNode n = mNodes.get(id);
        if (n != null) n.setRefDistance(dist);
    }

    // ─────────────────────────────────────────────────────────
    //  Bus / Master
    // ─────────────────────────────────────────────────────────
    public void setMasterVolume(float v) {
        mMasterVol = v;
        mListener.setVolume(v);
    }
    public void setMusicVolume(float v) {
        mMusicVol = v;
        if (mMusicId >= 0) setVolume(mMusicId, v);
    }
    public void setSfxVolume(float v) {
        mSfxVol = v;
    }

    // ─────────────────────────────────────────────────────────
    //  Music crossfade helper
    // ─────────────────────────────────────────────────────────
    public void playMusic(String assetPath, float fadeTime) {
        if (mMusicId >= 0) {
            // Fade out current (simplified – instant for now)
            stop(mMusicId);
        }
        mMusicId = createAudioNode(assetPath, false, true, true);
        play(mMusicId);
    }
    public void stopMusic() {
        if (mMusicId >= 0) { stop(mMusicId); mMusicId=-1; }
    }

    // ─────────────────────────────────────────────────────────
    //  Status
    // ─────────────────────────────────────────────────────────
    public String getStatusJSON() {
        StringBuilder sb = new StringBuilder("[");
        boolean first = true;
        for (var e : mNodes.entrySet()) {
            if (!first) sb.append(",");
            first = false;
            AudioNode n = e.getValue();
            sb.append("{\"id\":").append(e.getKey())
              .append(",\"vol\":").append(n.getVolume())
              .append(",\"pos\":").append(n.isPositional())
              .append(",\"loop\":").append(n.isLooping())
              .append("}");
        }
        sb.append("]");
        return sb.toString();
    }

    public void destroy() {
        for (AudioNode n : mNodes.values()) n.stop();
        mNodes.clear();
    }
}
