#pragma once
// ============================================================
//  ForgeEngine – JNIBridgeFull.h
//  Complete JNI bridge covering all subsystems.
//  Replaces JNIBridge.h as authoritative bridge header.
// ============================================================
#include <jni.h>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

struct JMETransform {
    float tx,ty,tz;      // translation
    float rx,ry,rz,rw;   // quaternion
    float sx,sy,sz;      // scale
};
enum class JMESpatialType {
    NODE=0,BOX,SPHERE,CYLINDER,CAPSULE,TORUS,PLANE,TERRAIN,
    LIGHT_POINT,LIGHT_DIRECTIONAL,LIGHT_SPOT,CAMERA,PARTICLE,AUDIO,UI_ELEMENT
};

// Shorthand macro to void-call Java with exception check
#define CALL_VOID(method, ...) \
    GetEnv()->CallVoidMethod(m_bridge, m_mc.method, ##__VA_ARGS__); \
    CheckException(#method);

// ── Full method ID cache ──────────────────────────────────────
struct FullMethodCache {
    // Core
    jmethodID init,startGame,pauseGame,stopGame,setEditorMode;
    jmethodID newScene,saveScene,loadScene,getSceneTreeJSON;
    jmethodID undo,redo,canUndo,canRedo,getAndClearLogs,renderPreviewFrame;
    // Spatials
    jmethodID addSpatial,removeSpatial,setVisible,setSpatialName,duplicateSpatial;
    jmethodID setSpatialTranslation,setSpatialRotation,setSpatialScale;
    jmethodID getSpatialTransform,reparent;
    // Materials
    jmethodID setMaterialColor,setMaterialTexture,setMaterialPBR,applyMaterialFile;
    // Lights
    jmethodID addLight,setLightColor,setLightIntensity;
    // Camera
    jmethodID setCameraTransform,setCameraFOV,getCameraTransform;
    // Animation
    jmethodID playAnimation,stopAnimation,setAnimSpeed,getAnimClipsJSON;
    // Physics
    jmethodID addRigidBody,removeRigidBody,setMass,setKinematic,setFriction;
    jmethodID setRestitution,applyImpulse,applyForce,setLinearVelocity;
    jmethodID getRigidBodyTransform,physicsRaycast,setGravity;
    jmethodID addCharacter,setWalkDirection,characterJump,isOnGround;
    // Audio
    jmethodID createAudio,attachAudio,playAudio,pauseAudio,stopAudio;
    jmethodID setAudioVolume,setAudioPitch,setAudioPosition;
    jmethodID playMusic,stopMusic,setMasterVolume,setMusicVolume,setSfxVolume;
    jmethodID getAudioStatusJSON,setListenerPosition;
    // Scripting
    jmethodID loadScript,registerScript,compileScript,getScriptError;
    jmethodID attachScript,detachScript,updateScriptSource,getScriptListJSON;
    // Network
    jmethodID hostServer,connectServer,disconnectNetwork;
    jmethodID sendNetworkPacket,broadcastNetworkPacket,broadcastTransform;
    jmethodID getNetworkStatusJSON,pingNetwork,sendChat;
    // Assets
    jmethodID getAssetListJSON,importAsset,buildProject;
    // Post-processing
    jmethodID setBloomEnabled,setBloomIntensity,setSSAOEnabled;
    jmethodID setDOFEnabled,setDOFFocusDistance,setDOFBlurScale,setFXAAEnabled;
    jmethodID setGridVisible,setShadowIntensity,setSunDirection;
};

// ─────────────────────────────────────────────────────────────
class JNIBridgeFull {
public:
    // ── Lifecycle ─────────────────────────────────────────────
    bool Init(JavaVM* vm, jobject activity);
    void Destroy();
    bool IsReady() const { return m_ready; }

    // ── Game control ──────────────────────────────────────────
    void StartGame();
    void PauseGame();
    void StopGame();
    void SetEditorMode(bool is3D);

    // ── Scene ─────────────────────────────────────────────────
    void        NewScene(const std::string& name);
    void        SaveScene(const std::string& path);
    bool        LoadScene(const std::string& path);
    std::string GetSceneTreeJSON();
    void        SyncSceneTree();

    // ── Spatials ──────────────────────────────────────────────
    int          AddSpatial(JMESpatialType type, const std::string& name, int parentId=-1);
    void         RemoveSpatial(int id);
    void         SetVisible(int id, bool visible);
    void         SetSpatialName(int id, const std::string& name);
    int          DuplicateSpatial(int id);
    void         SetTranslation(int id, float x, float y, float z);
    void         SetRotation(int id, float ex, float ey, float ez);  // euler deg → quat
    void         SetScale(int id, float x, float y, float z);
    void         Reparent(int id, int newParentId);
    JMETransform GetTransform(int id);

    // ── Materials ─────────────────────────────────────────────
    void SetMaterialColor(int id, float r, float g, float b, float a);
    void SetMaterialPBR(int id, float metallic, float roughness, float er, float eg, float eb);
    void SetMaterialTexture(int id, const std::string& slot, const std::string& path);
    void ApplyMaterialFile(int id, const std::string& j3mPath);

    // ── Lights ────────────────────────────────────────────────
    int  AddLight(int type, float r, float g, float b,
                  float x, float y, float z, int parentId=-1);
    void SetLightColor(int id, float r, float g, float b);
    void SetLightIntensity(int id, float intensity);

    // ── Camera ────────────────────────────────────────────────
    void SetCameraTransform(float tx, float ty, float tz,
                             float lx, float ly, float lz);
    void SetCameraFOV(float fovY);

    // ── Animation ─────────────────────────────────────────────
    void        PlayAnimation(int id, const std::string& clip, bool loop=true);
    void        StopAnimation(int id);
    void        SetAnimSpeed(int id, float speed);
    std::string GetAnimClipsJSON(int id);

    // ── Physics ───────────────────────────────────────────────
    int                AddRigidBody(int spatialId, int shapeType, float mass,
                                    const std::vector<float>& shapeData);
    void               RemoveRigidBody(int id);
    void               SetMass(int id, float mass);
    void               SetKinematic(int id, bool kinematic);
    void               SetFriction(int id, float friction);
    void               SetRestitution(int id, float restitution);
    void               ApplyImpulse(int id, float x, float y, float z);
    void               ApplyForce(int id, float x, float y, float z);
    void               SetLinearVelocity(int id, float x, float y, float z);
    void               SetGravity(float x, float y, float z);
    std::vector<float> PhysicsRaycast(float ox, float oy, float oz,
                                       float dx, float dy, float dz, float dist);
    int  AddCharacter(int spatialId, float radius, float height);
    void SetWalkDirection(int id, float x, float y, float z);
    void CharacterJump(int id);
    bool IsOnGround(int id);

    // ── Audio ─────────────────────────────────────────────────
    int  CreateAudio(const std::string& path, bool positional, bool stream, bool loop);
    void AttachAudio(int audioId, int spatialId);
    void PlayAudio(int id);
    void PauseAudio(int id);
    void StopAudio(int id);
    void SetAudioVolume(int id, float v);
    void SetAudioPitch(int id, float p);
    void SetAudioPosition(int id, float x, float y, float z);
    void PlayMusic(const std::string& path, float fadeTime=0.f);
    void StopMusic();
    void SetMasterVolume(float v);
    void SetMusicVolume(float v);
    void SetSfxVolume(float v);
    void SetListenerPosition(float x, float y, float z);

    // ── Scripting ─────────────────────────────────────────────
    int         RegisterScript(const std::string& name, const std::string& source);
    bool        CompileScript(int id);
    std::string GetScriptError(int id);
    void        AttachScript(int scriptId, int spatialId);
    void        DetachScript(int scriptId, int spatialId);
    void        UpdateScriptSource(int id, const std::string& source);
    std::string GetScriptListJSON();

    // ── Network ───────────────────────────────────────────────
    bool        HostServer(int port, int maxPlayers);
    bool        ConnectServer(const std::string& host, int port);
    void        DisconnectNetwork();
    void        BroadcastTransform(int entityId, float tx, float ty, float tz,
                                    float rx, float ry, float rz, float rw);
    std::string GetNetworkStatusJSON();
    void        PingNetwork();           // ping all connected peers
    void        SendChat(const std::string& msg);

    // ── Assets / Build ────────────────────────────────────────
    std::string GetAssetListJSON(const std::string& path);
    bool        ImportAsset(const std::string& src, const std::string& dst);
    void        BuildProject(const std::string& outputPath, const std::string& platform,
                              std::function<void(int, const std::string&)> cb);

    // ── Post-processing ───────────────────────────────────────
    void SetBloomEnabled(bool on);
    void SetBloomIntensity(float v);
    void SetSSAOEnabled(bool on);
    void SetDOFEnabled(bool on);
    void SetDOFFocusDistance(float d);
    void SetFXAAEnabled(bool on);
    void SetGridVisible(bool on);
    void SetShadowIntensity(float v);
    void SetSunDirection(float x, float y, float z);

    // ── Undo/Redo/Logs ────────────────────────────────────────
    void Undo();
    void Redo();
    bool CanUndo();
    bool CanRedo();
    void         PollLogs();
    unsigned int RenderPreviewFrame(int w, int h);

    // ── JNI helpers (public for native callbacks) ─────────────
    JNIEnv*     GetEnv();
    void        AttachThread();
    void        DetachThread();
    std::string J2S(jstring s);
    jstring     S2J(const std::string& s);

    // Build callback storage
    std::function<void(int,const std::string&)> m_buildCallback;

private:
    JavaVM*          m_vm     = nullptr;
    jobject          m_bridge = nullptr;
    jclass           m_class  = nullptr;
    FullMethodCache  m_mc     = {};
    bool             m_ready  = false;

    bool CacheMethodIDs();
    void CheckException(const char* context);
    void EulerToQuat(float ex, float ey, float ez,
                     float& qx, float& qy, float& qz, float& qw);
};

// ── Global accessor ───────────────────────────────────────────
JNIBridgeFull& GJNI();
