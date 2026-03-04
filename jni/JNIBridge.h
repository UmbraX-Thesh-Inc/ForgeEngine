#pragma once
// ============================================================
//  ForgeEngine  –  JNIBridge.h
//  C++ ↔ JMonkeyEngine (Java) communication layer.
//  All JME calls go through this bridge.
// ============================================================

#include <jni.h>
#include <string>
#include <vector>
#include <functional>
#include "../editor/ForgeEditor.h"

// ─── JNI method IDs cache ─────────────────────────────────────
struct JMEMethodCache {
    // com.forgeengine.EditorBridge
    jmethodID init;
    jmethodID startGame;
    jmethodID pauseGame;
    jmethodID stopGame;
    jmethodID saveScene;
    jmethodID loadScene;
    jmethodID newScene;
    jmethodID addSpatial;
    jmethodID removeSpatial;
    jmethodID setSpatialTranslation;
    jmethodID setSpatialRotation;
    jmethodID setSpatialScale;
    jmethodID getSpatialTransform;
    jmethodID getSceneTree;
    jmethodID buildProject;
    jmethodID getAssetList;
    jmethodID playAnimation;
    jmethodID stopAnimation;
    jmethodID setAnimSpeed;
    jmethodID getLogMessages;
    jmethodID renderFrame;        // returns texture id for preview
    jmethodID setEditorMode;      // 3D / 2D
    jmethodID undo;
    jmethodID redo;
};

// ─── Spatial type enum (mirrors JME) ─────────────────────────
enum class JMESpatialType {
    NODE, BOX, SPHERE, CYLINDER, CAPSULE,
    TORUS, PLANE, TERRAIN, LIGHT_POINT,
    LIGHT_DIRECTIONAL, LIGHT_SPOT, CAMERA,
    PARTICLE, AUDIO, UI_ELEMENT
};

// ─── Transform data packet ───────────────────────────────────
struct JMETransform {
    float tx, ty, tz;     // translation
    float rx, ry, rz, rw; // quaternion rotation
    float sx, sy, sz;     // scale
};

// ─── JNI Bridge class ─────────────────────────────────────────
class JNIBridge {
public:
    // ── Lifecycle ──────────────────────────────────────────────
    bool Init(JavaVM* vm, jobject activity);
    void Destroy();
    bool IsReady() const { return m_ready; }

    // ── Engine control ─────────────────────────────────────────
    void StartGame();
    void PauseGame();
    void StopGame();

    // ── Scene ──────────────────────────────────────────────────
    void     NewScene(const std::string& name);
    void     SaveScene(const std::string& path);
    bool     LoadScene(const std::string& path);

    // Returns JSON string of scene tree
    std::string GetSceneTreeJSON();

    // Parses scene tree JSON → fills GEditor().sceneNodes
    void     SyncSceneTree();

    // ── Spatials ───────────────────────────────────────────────
    int  AddSpatial(JMESpatialType type, const std::string& name,
                    int parentId = -1);
    void RemoveSpatial(int id);
    void SetTranslation(int id, float x, float y, float z);
    void SetRotation(int id, float x, float y, float z);  // euler→quat
    void SetScale(int id, float x, float y, float z);
    JMETransform GetTransform(int id);

    // ── Animations ─────────────────────────────────────────────
    void PlayAnimation(int spatialId, const std::string& clipName,
                       bool loop = true);
    void StopAnimation(int spatialId);
    void SetAnimSpeed(int spatialId, float speed);
    std::vector<std::string> GetAnimationClips(int spatialId);

    // ── Rendering ──────────────────────────────────────────────
    // Renders one frame into off-screen buffer,
    // returns OpenGL texture ID (or 0 on fail)
    unsigned int RenderPreviewFrame(int width, int height);

    // ── Assets ─────────────────────────────────────────────────
    std::string GetAssetListJSON(const std::string& path);

    // ── Build ──────────────────────────────────────────────────
    void BuildProject(const std::string& outputPath,
                      const std::string& platform,   // android | desktop
                      std::function<void(int progress, const std::string& msg)> cb);

    // ── Editor mode ────────────────────────────────────────────
    void SetEditorMode(bool is3D);   // true=3D  false=2D

    // ── Undo / Redo ────────────────────────────────────────────
    void Undo();
    void Redo();

    // ── Logging ───────────────────────────────────────────────
    // Pull new log lines from JME and append to GEditor().logs
    void PollLogs();

    // ── Raw JNI helpers ───────────────────────────────────────
    JNIEnv* GetEnv();
    void    AttachThread();
    void    DetachThread();

private:
    JavaVM*  m_vm       = nullptr;
    JNIEnv*  m_env      = nullptr;
    jobject  m_bridge   = nullptr;  // EditorBridge Java object
    jclass   m_class    = nullptr;
    JMEMethodCache m_mc = {};
    bool     m_ready    = false;

    bool  CacheMethodIDs();
    void  CheckException(const char* context);
    std::string JStringToStd(jstring s);
    jstring     StdToJString(const std::string& s);

    // Euler (deg) → quaternion
    void EulerToQuat(float ex, float ey, float ez,
                     float& qx, float& qy, float& qz, float& qw);
};
