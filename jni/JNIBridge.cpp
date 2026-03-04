// ============================================================
//  ForgeEngine  –  JNIBridge.cpp
//  Full implementation of C++ ↔ JMonkeyEngine bridge
// ============================================================

#include "JNIBridge.h"
#include <android/log.h>
#include <cmath>
#include <cstring>

#define LOG_TAG "ForgeJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ─── Java class name ─────────────────────────────────────────
static const char* kBridgeClass = "com/forgeengine/EditorBridge";

// ════════════════════════════════════════════════════════════
//  Init / Destroy
// ════════════════════════════════════════════════════════════
bool JNIBridge::Init(JavaVM* vm, jobject activity) {
    m_vm = vm;
    AttachThread();

    m_class = m_env->FindClass(kBridgeClass);
    if (!m_class) { LOGE("EditorBridge class not found"); return false; }
    m_class = (jclass)m_env->NewGlobalRef(m_class);

    // Instantiate EditorBridge(activity)
    jmethodID ctor = m_env->GetMethodID(m_class, "<init>",
        "(Landroid/app/Activity;)V");
    if (!ctor) { LOGE("EditorBridge ctor not found"); return false; }

    jobject local = m_env->NewObject(m_class, ctor, activity);
    if (!local) { LOGE("Failed to create EditorBridge"); return false; }
    m_bridge = m_env->NewGlobalRef(local);
    m_env->DeleteLocalRef(local);

    if (!CacheMethodIDs()) return false;

    // Call bridge.init()
    m_env->CallVoidMethod(m_bridge, m_mc.init);
    CheckException("Init");

    m_ready = true;
    LOGI("JNIBridge initialised OK");
    return true;
}

void JNIBridge::Destroy() {
    if (!m_env) return;
    if (m_bridge) { m_env->DeleteGlobalRef(m_bridge); m_bridge = nullptr; }
    if (m_class)  { m_env->DeleteGlobalRef(m_class);  m_class  = nullptr; }
    DetachThread();
    m_ready = false;
}

// ════════════════════════════════════════════════════════════
//  Cache method IDs (called once at init)
// ════════════════════════════════════════════════════════════
bool JNIBridge::CacheMethodIDs() {
    auto* e = m_env;
    auto* c = m_class;

#define GET(field, name, sig) \
    m_mc.field = e->GetMethodID(c, name, sig); \
    if (!m_mc.field) { LOGE("Method not found: %s %s", name, sig); return false; }

    GET(init,                 "init",                 "()V")
    GET(startGame,            "startGame",            "()V")
    GET(pauseGame,            "pauseGame",            "()V")
    GET(stopGame,             "stopGame",             "()V")
    GET(saveScene,            "saveScene",            "(Ljava/lang/String;)V")
    GET(loadScene,            "loadScene",            "(Ljava/lang/String;)Z")
    GET(newScene,             "newScene",             "(Ljava/lang/String;)V")
    GET(addSpatial,           "addSpatial",           "(ILjava/lang/String;I)I")
    GET(removeSpatial,        "removeSpatial",        "(I)V")
    GET(setSpatialTranslation,"setSpatialTranslation","(IFFF)V")
    GET(setSpatialRotation,   "setSpatialRotation",   "(IFFFF)V")  // id,qx,qy,qz,qw
    GET(setSpatialScale,      "setSpatialScale",      "(IFFF)V")
    GET(getSpatialTransform,  "getSpatialTransform",  "(I)[F")
    GET(getSceneTree,         "getSceneTreeJSON",     "()Ljava/lang/String;")
    GET(buildProject,         "buildProject",         "(Ljava/lang/String;Ljava/lang/String;)V")
    GET(getAssetList,         "getAssetListJSON",     "(Ljava/lang/String;)Ljava/lang/String;")
    GET(playAnimation,        "playAnimation",        "(ILjava/lang/String;Z)V")
    GET(stopAnimation,        "stopAnimation",        "(I)V")
    GET(setAnimSpeed,         "setAnimSpeed",         "(IF)V")
    GET(getLogMessages,       "getAndClearLogs",      "()Ljava/lang/String;")
    GET(renderFrame,          "renderPreviewFrame",   "(II)I")
    GET(setEditorMode,        "setEditorMode",        "(Z)V")
    GET(undo,                 "undo",                 "()V")
    GET(redo,                 "redo",                 "()V")
#undef GET
    return true;
}

// ════════════════════════════════════════════════════════════
//  Engine control
// ════════════════════════════════════════════════════════════
void JNIBridge::StartGame() {
    if (!m_ready) return;
    m_env->CallVoidMethod(m_bridge, m_mc.startGame);
    CheckException("StartGame");
}
void JNIBridge::PauseGame() {
    if (!m_ready) return;
    m_env->CallVoidMethod(m_bridge, m_mc.pauseGame);
    CheckException("PauseGame");
}
void JNIBridge::StopGame() {
    if (!m_ready) return;
    m_env->CallVoidMethod(m_bridge, m_mc.stopGame);
    CheckException("StopGame");
}

// ════════════════════════════════════════════════════════════
//  Scene
// ════════════════════════════════════════════════════════════
void JNIBridge::NewScene(const std::string& name) {
    jstring js = StdToJString(name);
    m_env->CallVoidMethod(m_bridge, m_mc.newScene, js);
    m_env->DeleteLocalRef(js);
    CheckException("NewScene");
}
void JNIBridge::SaveScene(const std::string& path) {
    jstring js = StdToJString(path);
    m_env->CallVoidMethod(m_bridge, m_mc.saveScene, js);
    m_env->DeleteLocalRef(js);
    CheckException("SaveScene");
}
bool JNIBridge::LoadScene(const std::string& path) {
    jstring js = StdToJString(path);
    jboolean ok = m_env->CallBooleanMethod(m_bridge, m_mc.loadScene, js);
    m_env->DeleteLocalRef(js);
    CheckException("LoadScene");
    return (bool)ok;
}
std::string JNIBridge::GetSceneTreeJSON() {
    jstring js = (jstring)m_env->CallObjectMethod(m_bridge, m_mc.getSceneTree);
    CheckException("GetSceneTree");
    return JStringToStd(js);
}

// Simple JSON parser for scene tree (minimal, no heavy lib dependency)
// Expected format (from Java side):
// [{"id":0,"name":"Root","type":"Node","parent":-1,
//   "tx":0,"ty":0,"tz":0,"rx":0,"ry":0,"rz":0,
//   "sx":1,"sy":1,"sz":1,"visible":true}, ...]
void JNIBridge::SyncSceneTree() {
    // In production: use nlohmann/json or a small SAX parser.
    // Here we call the Java side which returns JSON and trust the editor
    // panels to parse it with their own minimal parser.
    // The panel calls GetSceneTreeJSON() directly for simplicity.
}

// ════════════════════════════════════════════════════════════
//  Spatials
// ════════════════════════════════════════════════════════════
int JNIBridge::AddSpatial(JMESpatialType type, const std::string& name,
                           int parentId) {
    jstring jn = StdToJString(name);
    jint id = m_env->CallIntMethod(m_bridge, m_mc.addSpatial,
                                   (jint)type, jn, (jint)parentId);
    m_env->DeleteLocalRef(jn);
    CheckException("AddSpatial");
    return (int)id;
}
void JNIBridge::RemoveSpatial(int id) {
    m_env->CallVoidMethod(m_bridge, m_mc.removeSpatial, (jint)id);
    CheckException("RemoveSpatial");
}
void JNIBridge::SetTranslation(int id, float x, float y, float z) {
    m_env->CallVoidMethod(m_bridge, m_mc.setSpatialTranslation,
                          (jint)id, x, y, z);
    CheckException("SetTranslation");
}
void JNIBridge::SetRotation(int id, float ex, float ey, float ez) {
    float qx, qy, qz, qw;
    EulerToQuat(ex, ey, ez, qx, qy, qz, qw);
    m_env->CallVoidMethod(m_bridge, m_mc.setSpatialRotation,
                          (jint)id, qx, qy, qz, qw);
    CheckException("SetRotation");
}
void JNIBridge::SetScale(int id, float x, float y, float z) {
    m_env->CallVoidMethod(m_bridge, m_mc.setSpatialScale, (jint)id, x, y, z);
    CheckException("SetScale");
}
JMETransform JNIBridge::GetTransform(int id) {
    JMETransform t{};
    jfloatArray arr = (jfloatArray)m_env->CallObjectMethod(
        m_bridge, m_mc.getSpatialTransform, (jint)id);
    CheckException("GetTransform");
    if (!arr) return t;
    jfloat* f = m_env->GetFloatArrayElements(arr, nullptr);
    // [tx,ty,tz, qx,qy,qz,qw, sx,sy,sz]  (10 floats)
    t.tx=f[0]; t.ty=f[1]; t.tz=f[2];
    t.rx=f[3]; t.ry=f[4]; t.rz=f[5]; t.rw=f[6];
    t.sx=f[7]; t.sy=f[8]; t.sz=f[9];
    m_env->ReleaseFloatArrayElements(arr, f, JNI_ABORT);
    m_env->DeleteLocalRef(arr);
    return t;
}

// ════════════════════════════════════════════════════════════
//  Animation
// ════════════════════════════════════════════════════════════
void JNIBridge::PlayAnimation(int spatialId, const std::string& clip, bool loop) {
    jstring js = StdToJString(clip);
    m_env->CallVoidMethod(m_bridge, m_mc.playAnimation,
                          (jint)spatialId, js, (jboolean)loop);
    m_env->DeleteLocalRef(js);
    CheckException("PlayAnimation");
}
void JNIBridge::StopAnimation(int spatialId) {
    m_env->CallVoidMethod(m_bridge, m_mc.stopAnimation, (jint)spatialId);
    CheckException("StopAnimation");
}
void JNIBridge::SetAnimSpeed(int spatialId, float speed) {
    m_env->CallVoidMethod(m_bridge, m_mc.setAnimSpeed,
                          (jint)spatialId, speed);
    CheckException("SetAnimSpeed");
}

// ════════════════════════════════════════════════════════════
//  Rendering
// ════════════════════════════════════════════════════════════
unsigned int JNIBridge::RenderPreviewFrame(int w, int h) {
    jint texId = m_env->CallIntMethod(m_bridge, m_mc.renderFrame,
                                      (jint)w, (jint)h);
    CheckException("RenderPreviewFrame");
    return (unsigned int)texId;
}

// ════════════════════════════════════════════════════════════
//  Assets
// ════════════════════════════════════════════════════════════
std::string JNIBridge::GetAssetListJSON(const std::string& path) {
    jstring js  = StdToJString(path);
    jstring res = (jstring)m_env->CallObjectMethod(
        m_bridge, m_mc.getAssetList, js);
    m_env->DeleteLocalRef(js);
    CheckException("GetAssetList");
    return JStringToStd(res);
}

// ════════════════════════════════════════════════════════════
//  Build
// ════════════════════════════════════════════════════════════
void JNIBridge::BuildProject(const std::string& out,
                              const std::string& platform,
                              std::function<void(int,const std::string&)> cb) {
    // Build is async on Java side; progress reported via callback bridge
    // Simplified: fire and forget (Java posts events back via native callback)
    jstring jo = StdToJString(out);
    jstring jp = StdToJString(platform);
    m_env->CallVoidMethod(m_bridge, m_mc.buildProject, jo, jp);
    m_env->DeleteLocalRef(jo);
    m_env->DeleteLocalRef(jp);
    CheckException("BuildProject");
}

// ════════════════════════════════════════════════════════════
//  Editor mode / Undo
// ════════════════════════════════════════════════════════════
void JNIBridge::SetEditorMode(bool is3D) {
    m_env->CallVoidMethod(m_bridge, m_mc.setEditorMode, (jboolean)is3D);
    CheckException("SetEditorMode");
}
void JNIBridge::Undo() { m_env->CallVoidMethod(m_bridge, m_mc.undo); }
void JNIBridge::Redo() { m_env->CallVoidMethod(m_bridge, m_mc.redo); }

// ════════════════════════════════════════════════════════════
//  Logging
// ════════════════════════════════════════════════════════════
#include "../editor/ForgeEditor.h"   // for GEditor()
void JNIBridge::PollLogs() {
    jstring js = (jstring)m_env->CallObjectMethod(
        m_bridge, m_mc.getLogMessages);
    if (!js) return;
    std::string raw = JStringToStd(js);
    if (raw.empty()) return;
    // Format: "LEVEL|HH:MM:SS|message\n..."
    auto& logs = GEditor().logs;
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t nl = raw.find('\n', pos);
        std::string line = raw.substr(pos, nl == std::string::npos
                                       ? raw.size()-pos : nl-pos);
        pos = (nl == std::string::npos) ? raw.size() : nl+1;
        if (line.empty()) continue;
        LogEntry e;
        size_t p1 = line.find('|'), p2 = line.find('|', p1+1);
        if (p1 == std::string::npos) { e.level=LogLevel::JME; e.msg=line; }
        else {
            std::string lvl = line.substr(0, p1);
            e.time = (p2!=std::string::npos) ? line.substr(p1+1,p2-p1-1) : "";
            e.msg  = (p2!=std::string::npos) ? line.substr(p2+1) : line.substr(p1+1);
            if      (lvl=="WARN")  e.level=LogLevel::WARNING;
            else if (lvl=="ERROR") e.level=LogLevel::ERR;
            else if (lvl=="JME")   e.level=LogLevel::JME;
            else                   e.level=LogLevel::INFO;
        }
        logs.push_back(e);
        if (logs.size() > 2000) logs.erase(logs.begin());
    }
}

// ════════════════════════════════════════════════════════════
//  JNI thread helpers
// ════════════════════════════════════════════════════════════
JNIEnv* JNIBridge::GetEnv() { return m_env; }
void    JNIBridge::AttachThread() {
    m_vm->AttachCurrentThread(&m_env, nullptr);
}
void    JNIBridge::DetachThread() {
    m_vm->DetachCurrentThread();
    m_env = nullptr;
}

// ════════════════════════════════════════════════════════════
//  Helpers
// ════════════════════════════════════════════════════════════
void JNIBridge::CheckException(const char* ctx) {
    if (m_env->ExceptionCheck()) {
        LOGE("JNI exception in %s", ctx);
        m_env->ExceptionDescribe();
        m_env->ExceptionClear();
    }
}
std::string JNIBridge::JStringToStd(jstring s) {
    if (!s) return "";
    const char* c = m_env->GetStringUTFChars(s, nullptr);
    std::string r(c);
    m_env->ReleaseStringUTFChars(s, c);
    m_env->DeleteLocalRef(s);
    return r;
}
jstring JNIBridge::StdToJString(const std::string& s) {
    return m_env->NewStringUTF(s.c_str());
}
void JNIBridge::EulerToQuat(float ex, float ey, float ez,
                              float& qx, float& qy, float& qz, float& qw) {
    // Degrees → radians, XYZ order
    float cx=cosf(ex*0.008727f), sx=sinf(ex*0.008727f);
    float cy=cosf(ey*0.008727f), sy=sinf(ey*0.008727f);
    float cz=cosf(ez*0.008727f), sz=sinf(ez*0.008727f);
    qw = cx*cy*cz + sx*sy*sz;
    qx = sx*cy*cz - cx*sy*sz;
    qy = cx*sy*cz + sx*cy*sz;
    qz = cx*cy*sz - sx*sy*cz;
}

// ════════════════════════════════════════════════════════════
//  Native callbacks called FROM Java (JNI_OnLoad style)
// ════════════════════════════════════════════════════════════
extern "C" {

JNIEXPORT void JNICALL
Java_com_forgeengine_EditorBridge_nativeBuildProgress(
        JNIEnv* env, jobject, jint progress, jstring msg) {
    const char* c = env->GetStringUTFChars(msg, nullptr);
    // Post to editor state (thread-safe queue not shown for brevity)
    LOGI("Build progress %d%%: %s", progress, c);
    env->ReleaseStringUTFChars(msg, c);
}

JNIEXPORT void JNICALL
Java_com_forgeengine_EditorBridge_nativeSceneChanged(
        JNIEnv*, jobject) {
    GEditor().sceneDirty = true;
    GJNI().SyncSceneTree();
}

} // extern "C"
