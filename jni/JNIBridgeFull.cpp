// ============================================================
//  ForgeEngine – JNIBridgeFull.cpp
//  Complete C++ ↔ Java bridge implementation.
//  Covers ALL subsystems in EditorBridgeFull.java.
// ============================================================
#include "JNIBridgeFull.h"
#include "../editor/ForgeEditor.h"
#include "imgui.h"
#include <android/log.h>
#include <cmath>
#include <sstream>
#include <string>

#define TAG "ForgeJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// ── Global singleton ─────────────────────────────────────────
static JNIBridgeFull g_bridge;
JNIBridgeFull& GJNI() { return g_bridge; }

// ═══════════════════════════════════════════════════════════
//  Init / Destroy
// ═══════════════════════════════════════════════════════════
bool JNIBridgeFull::Init(JavaVM* vm, jobject activity) {
    m_vm = vm;
    JNIEnv* env = GetEnv();
    if (!env) { LOGE("GetEnv failed"); return false; }

    // Find EditorBridgeFull class
    jclass localClass = env->FindClass(
        "com/forgeengine/editor/EditorBridgeFull");
    if (!localClass) { LOGE("Class not found"); return false; }
    m_class = (jclass)env->NewGlobalRef(localClass);
    env->DeleteLocalRef(localClass);

    // Construct EditorBridgeFull(Activity)
    jmethodID ctor = env->GetMethodID(m_class, "<init>",
        "(Landroid/app/Activity;)V");
    jobject localBridge = env->NewObject(m_class, ctor, activity);
    m_bridge = env->NewGlobalRef(localBridge);
    env->DeleteLocalRef(localBridge);

    if (!CacheMethodIDs()) { LOGE("CacheMethodIDs failed"); return false; }

    // Call init()
    env->CallVoidMethod(m_bridge, m_mc.init);
    CheckException("init");

    m_ready = true;
    LOGI("JNIBridgeFull ready");
    return true;
}

void JNIBridgeFull::Destroy() {
    JNIEnv* env = GetEnv();
    if (env && m_bridge) env->DeleteGlobalRef(m_bridge);
    if (env && m_class)  env->DeleteGlobalRef(m_class);
    m_ready = false;
}

// ═══════════════════════════════════════════════════════════
//  Method ID cache
// ═══════════════════════════════════════════════════════════
#define GET(name, sig) \
    m_mc.name = env->GetMethodID(m_class, #name, sig); \
    if (!m_mc.name) { LOGE("Missing method: " #name); ok=false; }

bool JNIBridgeFull::CacheMethodIDs() {
    JNIEnv* env = GetEnv();
    bool ok = true;

    // Core
    GET(init,             "()V")
    GET(startGame,        "()V")
    GET(pauseGame,        "()V")
    GET(stopGame,         "()V")
    GET(setEditorMode,    "(Z)V")
    GET(newScene,         "(Ljava/lang/String;)V")
    GET(saveScene,        "(Ljava/lang/String;)V")
    GET(loadScene,        "(Ljava/lang/String;)Z")
    GET(getSceneTreeJSON, "()Ljava/lang/String;")
    GET(undo,             "()V")
    GET(redo,             "()V")
    GET(canUndo,          "()Z")
    GET(canRedo,          "()Z")
    GET(getAndClearLogs,  "()Ljava/lang/String;")
    GET(renderPreviewFrame,"(II)I")

    // Spatials
    GET(addSpatial,              "(ILjava/lang/String;I)I")
    GET(removeSpatial,           "(I)V")
    GET(setVisible,              "(IZ)V")
    GET(setSpatialName,          "(ILjava/lang/String;)V")
    GET(duplicateSpatial,        "(I)I")
    GET(setSpatialTranslation,   "(IFFF)V")
    GET(setSpatialRotation,      "(IFFFF)V")
    GET(setSpatialScale,         "(IFFF)V")
    GET(getSpatialTransform,     "(I)[F")
    GET(reparent,                "(II)V")

    // Materials
    GET(setMaterialColor,        "(IFFFF)V")
    GET(setMaterialTexture,      "(ILjava/lang/String;Ljava/lang/String;)V")
    GET(setMaterialPBR,          "(IFFFF)V")
    GET(applyMaterialFile,       "(ILjava/lang/String;)V")

    // Lights
    GET(addLight,                "(IFFFFFFF I)I") // type,r,g,b,x,y,z,parentId → fixed below
    GET(setLightColor,           "(IFFF)V")
    GET(setLightIntensity,       "(IF)V")

    // Camera
    GET(setCameraTransform,      "(FFFFFF)V")
    GET(setCameraFOV,            "(F)V")
    GET(getCameraTransform,      "()[F")

    // Animation
    GET(playAnimation,           "(ILjava/lang/String;Z)V")
    GET(stopAnimation,           "(I)V")
    GET(setAnimSpeed,            "(IF)V")
    GET(getAnimClipsJSON,        "(I)Ljava/lang/String;")

    // Physics
    GET(addRigidBody,            "(II F[F)I")  // spatialId, shapeType, mass, shapeData
    GET(removeRigidBody,         "(I)V")
    GET(setMass,                 "(IF)V")
    GET(setKinematic,            "(IZ)V")
    GET(setFriction,             "(IF)V")
    GET(setRestitution,          "(IF)V")
    GET(applyImpulse,            "(IFFF)V")
    GET(applyForce,              "(IFFF)V")
    GET(setLinearVelocity,       "(IFFF)V")
    GET(getRigidBodyTransform,   "(I)[F")
    GET(physicsRaycast,          "(FFFFFFF)[F")
    GET(setGravity,              "(FFF)V")
    GET(addCharacter,            "(IFF)I")
    GET(setWalkDirection,        "(IFFF)V")
    GET(characterJump,           "(I)V")
    GET(isOnGround,              "(I)Z")

    // Audio
    GET(createAudio,             "(Ljava/lang/String;ZZZ)I")
    GET(attachAudio,             "(II)V")
    GET(playAudio,               "(I)V")
    GET(pauseAudio,              "(I)V")
    GET(stopAudio,               "(I)V")
    GET(setAudioVolume,          "(IF)V")
    GET(setAudioPitch,           "(IF)V")
    GET(setAudioPosition,        "(IFFF)V")
    GET(playMusic,               "(Ljava/lang/String;F)V")
    GET(stopMusic,               "()V")
    GET(setMasterVolume,         "(F)V")
    GET(setMusicVolume,          "(F)V")
    GET(setSfxVolume,            "(F)V")
    GET(getAudioStatusJSON,      "()Ljava/lang/String;")
    GET(setListenerPosition,     "(FFF)V")

    // Scripting
    GET(loadScript,              "(Ljava/lang/String;)I")
    GET(registerScript,          "(Ljava/lang/String;Ljava/lang/String;)I")
    GET(compileScript,           "(I)Z")
    GET(getScriptError,          "(I)Ljava/lang/String;")
    GET(attachScript,            "(II)V")
    GET(detachScript,            "(II)V")
    GET(updateScriptSource,      "(ILjava/lang/String;)V")
    GET(getScriptListJSON,       "()Ljava/lang/String;")

    // Network
    GET(hostServer,              "(II)Z")
    GET(connectServer,           "(Ljava/lang/String;I)Z")
    GET(disconnectNetwork,       "()V")
    GET(sendNetworkPacket,       "(II[B)V")
    GET(broadcastNetworkPacket,  "(I[B)V")
    GET(broadcastTransform,      "(IFFFFFFF)V")
    GET(getNetworkStatusJSON,    "()Ljava/lang/String;")
    GET(pingNetwork,             "()V")
    GET(sendChat,                "(Ljava/lang/String;)V")

    // Assets
    GET(getAssetListJSON,        "(Ljava/lang/String;)Ljava/lang/String;")
    GET(importAsset,             "(Ljava/lang/String;Ljava/lang/String;)Z")

    // Build
    GET(buildProject,            "(Ljava/lang/String;Ljava/lang/String;)V")

    // Post-processing
    GET(setBloomEnabled,         "(Z)V")
    GET(setBloomIntensity,       "(F)V")
    GET(setSSAOEnabled,          "(Z)V")
    GET(setDOFEnabled,           "(Z)V")
    GET(setDOFFocusDistance,     "(F)V")
    GET(setDOFBlurScale,         "(F)V")
    GET(setFXAAEnabled,          "(Z)V")
    GET(setGridVisible,          "(Z)V")
    GET(setShadowIntensity,      "(F)V")
    GET(setSunDirection,         "(FFF)V")

    return ok;
}
#undef GET

// ═══════════════════════════════════════════════════════════
//  Core helpers
// ═══════════════════════════════════════════════════════════
JNIEnv* JNIBridgeFull::GetEnv() {
    JNIEnv* env = nullptr;
    if (m_vm->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_EDETACHED)
        m_vm->AttachCurrentThread(&env, nullptr);
    return env;
}

void JNIBridgeFull::AttachThread() {
    JNIEnv* env; m_vm->AttachCurrentThread(&env, nullptr);
}
void JNIBridgeFull::DetachThread() { m_vm->DetachCurrentThread(); }

void JNIBridgeFull::CheckException(const char* ctx) {
    JNIEnv* env = GetEnv();
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        LOGE("JNI exception in: %s", ctx);
    }
}

std::string JNIBridgeFull::J2S(jstring s) {
    if (!s) return "";
    JNIEnv* env = GetEnv();
    const char* c = env->GetStringUTFChars(s, nullptr);
    std::string r(c);
    env->ReleaseStringUTFChars(s, c);
    env->DeleteLocalRef(s);
    return r;
}
jstring JNIBridgeFull::S2J(const std::string& s) {
    return GetEnv()->NewStringUTF(s.c_str());
}

void JNIBridgeFull::EulerToQuat(float ex, float ey, float ez,
                                  float& qx, float& qy, float& qz, float& qw) {
    float cr=cosf(ex*0.5f), sr=sinf(ex*0.5f);
    float cp=cosf(ey*0.5f), sp=sinf(ey*0.5f);
    float cy=cosf(ez*0.5f), sy=sinf(ez*0.5f);
    qw = cr*cp*cy + sr*sp*sy;
    qx = sr*cp*cy - cr*sp*sy;
    qy = cr*sp*cy + sr*cp*sy;
    qz = cr*cp*sy - sr*sp*cy;
}

// ═══════════════════════════════════════════════════════════
//  Game control
// ═══════════════════════════════════════════════════════════
void JNIBridgeFull::StartGame() {
    GetEnv()->CallVoidMethod(m_bridge, m_mc.startGame);
    CheckException("StartGame");
}
void JNIBridgeFull::PauseGame() {
    GetEnv()->CallVoidMethod(m_bridge, m_mc.pauseGame);
    CheckException("PauseGame");
}
void JNIBridgeFull::StopGame() {
    GetEnv()->CallVoidMethod(m_bridge, m_mc.stopGame);
    CheckException("StopGame");
}
void JNIBridgeFull::SetEditorMode(bool is3D) {
    GetEnv()->CallVoidMethod(m_bridge, m_mc.setEditorMode, (jboolean)is3D);
    CheckException("SetEditorMode");
}

// ═══════════════════════════════════════════════════════════
//  Scene
// ═══════════════════════════════════════════════════════════
void JNIBridgeFull::NewScene(const std::string& name) {
    JNIEnv* env = GetEnv();
    env->CallVoidMethod(m_bridge, m_mc.newScene, S2J(name));
    CheckException("NewScene");
}
void JNIBridgeFull::SaveScene(const std::string& path) {
    JNIEnv* env = GetEnv();
    env->CallVoidMethod(m_bridge, m_mc.saveScene, S2J(path));
    CheckException("SaveScene");
}
bool JNIBridgeFull::LoadScene(const std::string& path) {
    JNIEnv* env = GetEnv();
    bool r = env->CallBooleanMethod(m_bridge, m_mc.loadScene, S2J(path));
    CheckException("LoadScene");
    return r;
}
std::string JNIBridgeFull::GetSceneTreeJSON() {
    JNIEnv* env = GetEnv();
    auto s = (jstring)env->CallObjectMethod(m_bridge, m_mc.getSceneTreeJSON);
    CheckException("GetSceneTreeJSON");
    return J2S(s);
}
void JNIBridgeFull::SyncSceneTree() {
    // Parse JSON into GEditor().sceneNodes
    std::string json = GetSceneTreeJSON();
    auto& nodes = GEditor().sceneNodes;
    nodes.clear();

    // Minimal JSON array parser (avoids needing a JSON lib in C++)
    // Format: [{"id":N,"name":"...","type":"...","parent":P,...},...]
    size_t pos = 0;
    auto readInt = [&](const char* key) -> int {
        std::string k = std::string("\"") + key + "\":";
        size_t p = json.find(k, pos);
        if (p == std::string::npos) return -1;
        p += k.size();
        return std::stoi(json.substr(p, json.find_first_of(",}", p)-p));
    };
    auto readFloat = [&](const char* key) -> float {
        std::string k = std::string("\"") + key + "\":";
        size_t p = json.find(k, pos);
        if (p == std::string::npos) return 0.f;
        p += k.size();
        return std::stof(json.substr(p, json.find_first_of(",}", p)-p));
    };
    auto readStr = [&](const char* key) -> std::string {
        std::string k = std::string("\"") + key + "\":\"";
        size_t p = json.find(k, pos);
        if (p == std::string::npos) return "";
        p += k.size();
        size_t e = json.find('"', p);
        return json.substr(p, e-p);
    };
    auto readBool = [&](const char* key) -> bool {
        std::string kt = std::string("\"") + key + "\":true";
        return json.find(kt, pos) != std::string::npos;
    };

    pos = 0;
    while (true) {
        size_t start = json.find('{', pos);
        if (start == std::string::npos) break;
        size_t end = json.find('}', start);
        if (end == std::string::npos) break;
        pos = start;

        SceneNode n;
        n.id          = readInt("id");
        n.name        = readStr("name");
        n.type        = readStr("type");
        int parentId  = readInt("parent");
        n.visible     = readBool("visible");
        n.translation[0] = readFloat("tx");
        n.translation[1] = readFloat("ty");
        n.translation[2] = readFloat("tz");
        n.rotation[0]    = readFloat("rx");
        n.rotation[1]    = readFloat("ry");
        n.rotation[2]    = readFloat("rz");
        n.scale[0]       = readFloat("sx");
        n.scale[1]       = readFloat("sy");
        n.scale[2]       = readFloat("sz");

        if (n.id >= 0) {
            if (parentId >= 0) {
                for (auto& p2 : nodes)
                    if (p2.id == parentId) { p2.childIds.push_back(n.id); break; }
            }
            nodes.push_back(n);
        }
        pos = end + 1;
    }
}

// ═══════════════════════════════════════════════════════════
//  Spatials
// ═══════════════════════════════════════════════════════════
int JNIBridgeFull::AddSpatial(JMESpatialType type, const std::string& name, int parentId) {
    JNIEnv* env = GetEnv();
    int r = env->CallIntMethod(m_bridge, m_mc.addSpatial,
        (jint)type, S2J(name), (jint)parentId);
    CheckException("AddSpatial");
    if (r > 0) SyncSceneTree();
    return r;
}
void JNIBridgeFull::RemoveSpatial(int id) {
    GetEnv()->CallVoidMethod(m_bridge, m_mc.removeSpatial, (jint)id);
    CheckException("RemoveSpatial");
    SyncSceneTree();
}
void JNIBridgeFull::SetVisible(int id, bool visible) {
    GetEnv()->CallVoidMethod(m_bridge, m_mc.setVisible, (jint)id, (jboolean)visible);
    CheckException("SetVisible");
}
void JNIBridgeFull::SetSpatialName(int id, const std::string& name) {
    jstring jname = S2J(name);
    GetEnv()->CallVoidMethod(m_bridge, m_mc.setSpatialName, (jint)id, jname);
    GetEnv()->DeleteLocalRef(jname);
    CheckException("SetSpatialName");
}
int JNIBridgeFull::DuplicateSpatial(int id) {
    int r = GetEnv()->CallIntMethod(m_bridge, m_mc.duplicateSpatial, (jint)id);
    CheckException("DuplicateSpatial");
    if (r > 0) SyncSceneTree();
    return r;
}
void JNIBridgeFull::SetTranslation(int id, float x, float y, float z) {
    GetEnv()->CallVoidMethod(m_bridge, m_mc.setSpatialTranslation,
        (jint)id, (jfloat)x, (jfloat)y, (jfloat)z);
    CheckException("SetTranslation");
}
void JNIBridgeFull::SetRotation(int id, float ex, float ey, float ez) {
    float qx,qy,qz,qw;
    EulerToQuat(ex*M_PI/180.f, ey*M_PI/180.f, ez*M_PI/180.f, qx,qy,qz,qw);
    GetEnv()->CallVoidMethod(m_bridge, m_mc.setSpatialRotation,
        (jint)id,(jfloat)qx,(jfloat)qy,(jfloat)qz,(jfloat)qw);
    CheckException("SetRotation");
}
void JNIBridgeFull::SetScale(int id, float x, float y, float z) {
    GetEnv()->CallVoidMethod(m_bridge, m_mc.setSpatialScale,
        (jint)id,(jfloat)x,(jfloat)y,(jfloat)z);
    CheckException("SetScale");
}
void JNIBridgeFull::Reparent(int id, int newParentId) {
    GetEnv()->CallVoidMethod(m_bridge, m_mc.reparent, (jint)id, (jint)newParentId);
    CheckException("Reparent");
    SyncSceneTree();
}
JMETransform JNIBridgeFull::GetTransform(int id) {
    JNIEnv* env = GetEnv();
    auto arr = (jfloatArray)env->CallObjectMethod(m_bridge, m_mc.getSpatialTransform, (jint)id);
    CheckException("GetTransform");
    JMETransform t{};
    if (!arr) return t;
    float* f = env->GetFloatArrayElements(arr, nullptr);
    t.tx=f[0]; t.ty=f[1]; t.tz=f[2];
    t.rx=f[3]; t.ry=f[4]; t.rz=f[5]; t.rw=f[6];
    t.sx=f[7]; t.sy=f[8]; t.sz=f[9];
    env->ReleaseFloatArrayElements(arr, f, 0);
    return t;
}

// ═══════════════════════════════════════════════════════════
//  Materials
// ═══════════════════════════════════════════════════════════
void JNIBridgeFull::SetMaterialColor(int id, float r, float g, float b, float a) {
    GetEnv()->CallVoidMethod(m_bridge, m_mc.setMaterialColor,
        (jint)id,(jfloat)r,(jfloat)g,(jfloat)b,(jfloat)a);
    CheckException("SetMaterialColor");
}
void JNIBridgeFull::SetMaterialPBR(int id, float metallic, float roughness,
                                    float er, float eg, float eb) {
    GetEnv()->CallVoidMethod(m_bridge, m_mc.setMaterialPBR,
        (jint)id,(jfloat)metallic,(jfloat)roughness,(jfloat)er,(jfloat)eg,(jfloat)eb);
    CheckException("SetMaterialPBR");
}
void JNIBridgeFull::SetMaterialTexture(int id, const std::string& slot, const std::string& path) {
    JNIEnv* env = GetEnv();
    env->CallVoidMethod(m_bridge, m_mc.setMaterialTexture,
        (jint)id, S2J(slot), S2J(path));
    CheckException("SetMaterialTexture");
}
void JNIBridgeFull::ApplyMaterialFile(int id, const std::string& j3mPath) {
    JNIEnv* env = GetEnv();
    env->CallVoidMethod(m_bridge, m_mc.applyMaterialFile, (jint)id, S2J(j3mPath));
    CheckException("ApplyMaterialFile");
}

// ═══════════════════════════════════════════════════════════
//  Lights
// ═══════════════════════════════════════════════════════════
int JNIBridgeFull::AddLight(int type, float r, float g, float b,
                              float x, float y, float z, int parentId) {
    JNIEnv* env = GetEnv();
    int id = env->CallIntMethod(m_bridge, m_mc.addLight,
        (jint)type,(jfloat)r,(jfloat)g,(jfloat)b,
        (jfloat)x,(jfloat)y,(jfloat)z,(jint)parentId);
    CheckException("AddLight");
    return id;
}
void JNIBridgeFull::SetLightColor(int id, float r, float g, float b) {
    GetEnv()->CallVoidMethod(m_bridge, m_mc.setLightColor,
        (jint)id,(jfloat)r,(jfloat)g,(jfloat)b);
    CheckException("SetLightColor");
}
void JNIBridgeFull::SetLightIntensity(int id, float intensity) {
    GetEnv()->CallVoidMethod(m_bridge, m_mc.setLightIntensity,
        (jint)id,(jfloat)intensity);
    CheckException("SetLightIntensity");
}

// ═══════════════════════════════════════════════════════════
//  Camera
// ═══════════════════════════════════════════════════════════
void JNIBridgeFull::SetCameraTransform(float tx, float ty, float tz,
                                        float lx, float ly, float lz) {
    GetEnv()->CallVoidMethod(m_bridge, m_mc.setCameraTransform,
        (jfloat)tx,(jfloat)ty,(jfloat)tz,
        (jfloat)lx,(jfloat)ly,(jfloat)lz);
    CheckException("SetCameraTransform");
}
void JNIBridgeFull::SetCameraFOV(float fov) {
    GetEnv()->CallVoidMethod(m_bridge, m_mc.setCameraFOV, (jfloat)fov);
    CheckException("SetCameraFOV");
}

// ═══════════════════════════════════════════════════════════
//  Animation
// ═══════════════════════════════════════════════════════════
void JNIBridgeFull::PlayAnimation(int id, const std::string& clip, bool loop) {
    JNIEnv* env = GetEnv();
    env->CallVoidMethod(m_bridge, m_mc.playAnimation,
        (jint)id, S2J(clip), (jboolean)loop);
    CheckException("PlayAnimation");
}
void JNIBridgeFull::StopAnimation(int id) {
    GetEnv()->CallVoidMethod(m_bridge, m_mc.stopAnimation, (jint)id);
    CheckException("StopAnimation");
}
void JNIBridgeFull::SetAnimSpeed(int id, float spd) {
    GetEnv()->CallVoidMethod(m_bridge, m_mc.setAnimSpeed, (jint)id, (jfloat)spd);
    CheckException("SetAnimSpeed");
}
std::string JNIBridgeFull::GetAnimClipsJSON(int id) {
    JNIEnv* env = GetEnv();
    auto s=(jstring)env->CallObjectMethod(m_bridge, m_mc.getAnimClipsJSON,(jint)id);
    CheckException("GetAnimClipsJSON");
    return J2S(s);
}

// ═══════════════════════════════════════════════════════════
//  Physics
// ═══════════════════════════════════════════════════════════
int JNIBridgeFull::AddRigidBody(int spatialId, int shapeType, float mass,
                                 const std::vector<float>& shapeData) {
    JNIEnv* env = GetEnv();
    jfloatArray jdata = env->NewFloatArray((jsize)shapeData.size());
    if (!shapeData.empty())
        env->SetFloatArrayRegion(jdata, 0, (jsize)shapeData.size(), shapeData.data());
    int r = env->CallIntMethod(m_bridge, m_mc.addRigidBody,
        (jint)spatialId,(jint)shapeType,(jfloat)mass, jdata);
    env->DeleteLocalRef(jdata);
    CheckException("AddRigidBody");
    return r;
}
void JNIBridgeFull::RemoveRigidBody(int id)              { CALL_VOID(removeRigidBody, (jint)id) }
void JNIBridgeFull::SetMass(int id, float m)             { CALL_VOID(setMass, (jint)id,(jfloat)m) }
void JNIBridgeFull::SetKinematic(int id, bool k)         { CALL_VOID(setKinematic, (jint)id,(jboolean)k) }
void JNIBridgeFull::SetFriction(int id, float f)         { CALL_VOID(setFriction, (jint)id,(jfloat)f) }
void JNIBridgeFull::SetRestitution(int id, float r)      { CALL_VOID(setRestitution, (jint)id,(jfloat)r) }
void JNIBridgeFull::ApplyImpulse(int id, float x, float y, float z) { CALL_VOID(applyImpulse, (jint)id,(jfloat)x,(jfloat)y,(jfloat)z) }
void JNIBridgeFull::ApplyForce(int id, float x, float y, float z)   { CALL_VOID(applyForce, (jint)id,(jfloat)x,(jfloat)y,(jfloat)z) }
void JNIBridgeFull::SetLinearVelocity(int id, float x, float y, float z) { CALL_VOID(setLinearVelocity,(jint)id,(jfloat)x,(jfloat)y,(jfloat)z) }
void JNIBridgeFull::SetGravity(float x, float y, float z) { CALL_VOID(setGravity,(jfloat)x,(jfloat)y,(jfloat)z) }

std::vector<float> JNIBridgeFull::PhysicsRaycast(float ox,float oy,float oz,
                                                   float dx,float dy,float dz,float dist) {
    JNIEnv* env = GetEnv();
    auto arr=(jfloatArray)env->CallObjectMethod(m_bridge,m_mc.physicsRaycast,
        (jfloat)ox,(jfloat)oy,(jfloat)oz,(jfloat)dx,(jfloat)dy,(jfloat)dz,(jfloat)dist);
    CheckException("PhysicsRaycast");
    if (!arr) return {};
    jsize len = env->GetArrayLength(arr);
    float* f = env->GetFloatArrayElements(arr, nullptr);
    std::vector<float> r(f, f+len);
    env->ReleaseFloatArrayElements(arr,f,0);
    return r;
}
int  JNIBridgeFull::AddCharacter(int spatialId, float radius, float height) {
    return GetEnv()->CallIntMethod(m_bridge,m_mc.addCharacter,(jint)spatialId,(jfloat)radius,(jfloat)height);
}
void JNIBridgeFull::SetWalkDirection(int id,float x,float y,float z) { CALL_VOID(setWalkDirection,(jint)id,(jfloat)x,(jfloat)y,(jfloat)z) }
void JNIBridgeFull::CharacterJump(int id) { CALL_VOID(characterJump,(jint)id) }
bool JNIBridgeFull::IsOnGround(int id)   { return GetEnv()->CallBooleanMethod(m_bridge,m_mc.isOnGround,(jint)id); }

// ═══════════════════════════════════════════════════════════
//  Audio
// ═══════════════════════════════════════════════════════════
int JNIBridgeFull::CreateAudio(const std::string& path, bool positional, bool stream, bool loop) {
    JNIEnv* env=GetEnv();
    return env->CallIntMethod(m_bridge,m_mc.createAudio,
        S2J(path),(jboolean)positional,(jboolean)stream,(jboolean)loop);
}
void JNIBridgeFull::AttachAudio(int audioId, int spatialId) { CALL_VOID(attachAudio,(jint)audioId,(jint)spatialId) }
void JNIBridgeFull::PlayAudio(int id)   { CALL_VOID(playAudio,(jint)id) }
void JNIBridgeFull::PauseAudio(int id)  { CALL_VOID(pauseAudio,(jint)id) }
void JNIBridgeFull::StopAudio(int id)   { CALL_VOID(stopAudio,(jint)id) }
void JNIBridgeFull::SetAudioVolume(int id,float v) { CALL_VOID(setAudioVolume,(jint)id,(jfloat)v) }
void JNIBridgeFull::SetAudioPitch(int id,float p)  { CALL_VOID(setAudioPitch,(jint)id,(jfloat)p) }
void JNIBridgeFull::SetAudioPosition(int id,float x,float y,float z) { CALL_VOID(setAudioPosition,(jint)id,(jfloat)x,(jfloat)y,(jfloat)z) }
void JNIBridgeFull::PlayMusic(const std::string& p,float f) { JNIEnv* e=GetEnv(); e->CallVoidMethod(m_bridge,m_mc.playMusic,S2J(p),(jfloat)f); }
void JNIBridgeFull::StopMusic()  { CALL_VOID(stopMusic) }
void JNIBridgeFull::SetMasterVolume(float v) { CALL_VOID(setMasterVolume,(jfloat)v) }
void JNIBridgeFull::SetMusicVolume(float v)  { CALL_VOID(setMusicVolume,(jfloat)v) }
void JNIBridgeFull::SetSfxVolume(float v)    { CALL_VOID(setSfxVolume,(jfloat)v) }
void JNIBridgeFull::SetListenerPosition(float x,float y,float z) { CALL_VOID(setListenerPosition,(jfloat)x,(jfloat)y,(jfloat)z) }

// ═══════════════════════════════════════════════════════════
//  Scripting
// ═══════════════════════════════════════════════════════════
int  JNIBridgeFull::RegisterScript(const std::string& name,const std::string& src){
    JNIEnv* e=GetEnv();
    return e->CallIntMethod(m_bridge,m_mc.registerScript,S2J(name),S2J(src));
}
bool JNIBridgeFull::CompileScript(int id){ return GetEnv()->CallBooleanMethod(m_bridge,m_mc.compileScript,(jint)id); }
std::string JNIBridgeFull::GetScriptError(int id){
    JNIEnv* e=GetEnv();
    return J2S((jstring)e->CallObjectMethod(m_bridge,m_mc.getScriptError,(jint)id));
}
void JNIBridgeFull::AttachScript(int sid,int spatialId){ CALL_VOID(attachScript,(jint)sid,(jint)spatialId) }
void JNIBridgeFull::DetachScript(int sid,int spatialId){ CALL_VOID(detachScript,(jint)sid,(jint)spatialId) }
void JNIBridgeFull::UpdateScriptSource(int id,const std::string& src){
    JNIEnv* e=GetEnv();
    e->CallVoidMethod(m_bridge,m_mc.updateScriptSource,(jint)id,S2J(src));
}
std::string JNIBridgeFull::GetScriptListJSON(){
    JNIEnv* e=GetEnv();
    return J2S((jstring)e->CallObjectMethod(m_bridge,m_mc.getScriptListJSON));
}

// ═══════════════════════════════════════════════════════════
//  Network
// ═══════════════════════════════════════════════════════════
bool JNIBridgeFull::HostServer(int port,int maxP){ return GetEnv()->CallBooleanMethod(m_bridge,m_mc.hostServer,(jint)port,(jint)maxP); }
bool JNIBridgeFull::ConnectServer(const std::string& h,int port){
    return GetEnv()->CallBooleanMethod(m_bridge,m_mc.connectServer,S2J(h),(jint)port);
}
void JNIBridgeFull::DisconnectNetwork(){ CALL_VOID(disconnectNetwork) }
void JNIBridgeFull::BroadcastTransform(int eid,float tx,float ty,float tz,
                                         float rx,float ry,float rz,float rw){
    GetEnv()->CallVoidMethod(m_bridge,m_mc.broadcastTransform,
        (jint)eid,(jfloat)tx,(jfloat)ty,(jfloat)tz,
        (jfloat)rx,(jfloat)ry,(jfloat)rz,(jfloat)rw);
    CheckException("BroadcastTransform");
}
std::string JNIBridgeFull::GetNetworkStatusJSON(){
    return J2S((jstring)GetEnv()->CallObjectMethod(m_bridge,m_mc.getNetworkStatusJSON));
}
void JNIBridgeFull::SendChat(const std::string& msg){ GetEnv()->CallVoidMethod(m_bridge,m_mc.sendChat,S2J(msg)); }
void JNIBridgeFull::PingNetwork(){
    JNIEnv* env = GetEnv();
    if (!env || !m_bridge) return;
    // NetworkManager.pingAll() via reflection fallback
    jclass cls = env->GetObjectClass(m_bridge);
    jmethodID mid = env->GetMethodID(cls,"pingAll","()V");
    if (mid) env->CallVoidMethod(m_bridge, mid);
    env->DeleteLocalRef(cls);
}

// ═══════════════════════════════════════════════════════════
//  Assets / Build
// ═══════════════════════════════════════════════════════════
std::string JNIBridgeFull::GetAssetListJSON(const std::string& path){
    JNIEnv* e=GetEnv();
    return J2S((jstring)e->CallObjectMethod(m_bridge,m_mc.getAssetListJSON,S2J(path)));
}
bool JNIBridgeFull::ImportAsset(const std::string& src,const std::string& dst){
    JNIEnv* e=GetEnv();
    return e->CallBooleanMethod(m_bridge,m_mc.importAsset,S2J(src),S2J(dst));
}
void JNIBridgeFull::BuildProject(const std::string& out,const std::string& plat,
                                   std::function<void(int,const std::string&)> cb){
    m_buildCallback = cb;
    JNIEnv* e=GetEnv();
    e->CallVoidMethod(m_bridge,m_mc.buildProject,S2J(out),S2J(plat));
    CheckException("BuildProject");
}

// ═══════════════════════════════════════════════════════════
//  Post-processing
// ═══════════════════════════════════════════════════════════
void JNIBridgeFull::SetBloomEnabled(bool on)         { CALL_VOID(setBloomEnabled,(jboolean)on) }
void JNIBridgeFull::SetBloomIntensity(float v)       { CALL_VOID(setBloomIntensity,(jfloat)v) }
void JNIBridgeFull::SetSSAOEnabled(bool on)          { CALL_VOID(setSSAOEnabled,(jboolean)on) }
void JNIBridgeFull::SetDOFEnabled(bool on)           { CALL_VOID(setDOFEnabled,(jboolean)on) }
void JNIBridgeFull::SetDOFFocusDistance(float d)     { CALL_VOID(setDOFFocusDistance,(jfloat)d) }
void JNIBridgeFull::SetFXAAEnabled(bool on)          { CALL_VOID(setFXAAEnabled,(jboolean)on) }
void JNIBridgeFull::SetGridVisible(bool on)          { CALL_VOID(setGridVisible,(jboolean)on) }
void JNIBridgeFull::SetShadowIntensity(float v)      { CALL_VOID(setShadowIntensity,(jfloat)v) }
void JNIBridgeFull::SetSunDirection(float x,float y,float z){ CALL_VOID(setSunDirection,(jfloat)x,(jfloat)y,(jfloat)z) }

// ═══════════════════════════════════════════════════════════
//  Undo/Redo/Logs/Preview
// ═══════════════════════════════════════════════════════════
void JNIBridgeFull::Undo() { CALL_VOID(undo) }
void JNIBridgeFull::Redo() { CALL_VOID(redo) }
bool JNIBridgeFull::CanUndo() { return GetEnv()->CallBooleanMethod(m_bridge,m_mc.canUndo); }
bool JNIBridgeFull::CanRedo() { return GetEnv()->CallBooleanMethod(m_bridge,m_mc.canRedo); }

void JNIBridgeFull::PollLogs() {
    JNIEnv* env = GetEnv();
    auto js = (jstring)env->CallObjectMethod(m_bridge, m_mc.getAndClearLogs);
    CheckException("PollLogs");
    std::string raw = J2S(js);
    if (raw.empty()) return;

    std::istringstream ss(raw);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        LogLevel lv = LogLevel::INFO;
        size_t p1 = line.find('|'), p2 = line.find('|', p1+1);
        std::string lvStr  = line.substr(0, p1);
        std::string timeStr = line.substr(p1+1, p2-p1-1);
        std::string msg    = line.substr(p2+1);
        if (lvStr=="ERROR") lv = LogLevel::ERR;
        else if (lvStr=="WARN") lv = LogLevel::WARNING;
        else if (lvStr=="JME") lv = LogLevel::JME;
        GEditor().logs.push_back({lv, msg, timeStr});
    }
}

unsigned int JNIBridgeFull::RenderPreviewFrame(int w, int h) {
    JNIEnv* env = GetEnv();
    int r = env->CallIntMethod(m_bridge, m_mc.renderPreviewFrame, (jint)w, (jint)h);
    CheckException("RenderPreviewFrame");
    return (unsigned int)r;
}

// ═══════════════════════════════════════════════════════════
//  Native callbacks  (called from Java)
// ═══════════════════════════════════════════════════════════
extern "C" {

JNIEXPORT void JNICALL
Java_com_forgeengine_editor_EditorBridgeFull_nativeBuildProgress(
        JNIEnv* env, jobject obj, jint pct, jstring msg) {
    std::string m = g_bridge.J2S(msg);
    if (g_bridge.m_buildCallback)
        g_bridge.m_buildCallback((int)pct, m);
    GEditor().logs.push_back({LogLevel::INFO,
        "Build " + std::to_string((int)pct) + "% – " + m, "now"});
}

JNIEXPORT void JNICALL
Java_com_forgeengine_editor_EditorBridgeFull_nativeSceneChanged(
        JNIEnv* env, jobject obj) {
    g_bridge.SyncSceneTree();
}

JNIEXPORT void JNICALL
Java_com_forgeengine_editor_EditorBridgeFull_nativePhysicsCollision(
        JNIEnv* env, jobject obj, jint idA, jint idB) {
    GEditor().logs.push_back({LogLevel::INFO,
        "Collision: " + std::to_string((int)idA) +
        " ↔ "         + std::to_string((int)idB), "now"});
}

JNIEXPORT void JNICALL
Java_com_forgeengine_editor_EditorBridgeFull_nativeNetworkPacket(
        JNIEnv* env, jobject obj, jint peerId, jint type, jbyteArray data) {
    // Forward to network panel / game logic
    GEditor().logs.push_back({LogLevel::INFO,
        "Net pkt type=" + std::to_string((int)type) +
        " from peer "   + std::to_string((int)peerId), "now"});
}

} // extern "C"

// ═══════════════════════════════════════════════════════════
//  ForgeActivity native method implementations
//  Java: private native void nativeOnCreate(Activity, String, String)
// ═══════════════════════════════════════════════════════════
extern "C" {

JNIEXPORT void JNICALL
Java_com_forgeengine_ForgeActivity_nativeOnCreate(
        JNIEnv* env, jobject thiz,
        jobject activity, jstring assetPath, jstring iconsPath) {
    JavaVM* vm = nullptr;
    env->GetJavaVM(&vm);
    g_bridge.Init(vm, activity);
    LOGI("nativeOnCreate: JNI bridge initialized");
}

JNIEXPORT void JNICALL
Java_com_forgeengine_ForgeActivity_nativeOnResume(
        JNIEnv* env, jobject thiz) {
    // Resume JME app state
    LOGI("nativeOnResume");
}

JNIEXPORT void JNICALL
Java_com_forgeengine_ForgeActivity_nativePause(
        JNIEnv* env, jobject thiz) {
    // Pause JME if playing
    LOGI("nativePause");
}

JNIEXPORT void JNICALL
Java_com_forgeengine_ForgeActivity_nativeOnStop(
        JNIEnv* env, jobject thiz) {
    LOGI("nativeOnStop");
}

JNIEXPORT void JNICALL
Java_com_forgeengine_ForgeActivity_nativeOnSurfaceCreated(
        JNIEnv* env, jobject thiz,
        jobject surface, jint w, jint h) {
    LOGI("nativeOnSurfaceCreated %dx%d", (int)w, (int)h);
}

JNIEXPORT void JNICALL
Java_com_forgeengine_ForgeActivity_nativeOnSurfaceChanged(
        JNIEnv* env, jobject thiz, jint w, jint h) {
    ImGui::GetIO().DisplaySize = {(float)w, (float)h};
    LOGI("nativeOnSurfaceChanged %dx%d", (int)w, (int)h);
}

JNIEXPORT void JNICALL
Java_com_forgeengine_ForgeActivity_nativeTouchEvent(
        JNIEnv* env, jobject thiz,
        jint action, jint ptrId, jfloat x, jfloat y, jfloat pressure) {
    ImGuiIO& io = ImGui::GetIO();
    switch (action) {
        case 0: // ACTION_DOWN
        case 5: // ACTION_POINTER_DOWN
            io.AddMousePosEvent(x, y);
            io.AddMouseButtonEvent(0, true);
            break;
        case 1: // ACTION_UP
        case 6: // ACTION_POINTER_UP
            io.AddMouseButtonEvent(0, false);
            break;
        case 2: // ACTION_MOVE
            io.AddMousePosEvent(x, y);
            break;
    }
}

JNIEXPORT void JNICALL
Java_com_forgeengine_ForgeActivity_nativeKeyDown(
        JNIEnv* env, jobject thiz, jint keyCode) {
    if (keyCode < 512)
        ImGui::GetIO().KeysDown[keyCode] = true;
}

JNIEXPORT void JNICALL
Java_com_forgeengine_ForgeActivity_nativeKeyUp(
        JNIEnv* env, jobject thiz, jint keyCode) {
    if (keyCode < 512)
        ImGui::GetIO().KeysDown[keyCode] = false;
}

JNIEXPORT void JNICALL
Java_com_forgeengine_ForgeActivity_nativeBackButton(
        JNIEnv* env, jobject thiz) {
    // Back = open settings or confirm exit
    GEditor().showSettingsModal = true;
}

} // extern "C" (ForgeActivity natives)
