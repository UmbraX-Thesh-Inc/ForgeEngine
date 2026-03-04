// ============================================================
//  ForgeEngine – android_main.cpp
//  Android NDK entry: EGL/OpenGL ES 3, ImGui, JNI bridge.
// ============================================================
#include <android_native_app_glue.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <string>

#include "imgui.h"
#include "imgui_impl_android.h"
#include "imgui_impl_opengl3.h"
#include "editor/ForgeEditor.h"
#include "jni/JNIBridgeFull.h"

#define TAG "ForgeMain"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static EGLDisplay g_display = EGL_NO_DISPLAY;
static EGLSurface g_surface = EGL_NO_SURFACE;
static EGLContext g_context = EGL_NO_CONTEXT;
static int        g_width = 0, g_height = 0;
static bool       g_ready = false;

// ── EGL ───────────────────────────────────────────────────────
static bool InitEGL(android_app* app) {
    g_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(g_display, nullptr, nullptr);
    const EGLint attr[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RED_SIZE,8, EGL_GREEN_SIZE,8, EGL_BLUE_SIZE,8,
        EGL_DEPTH_SIZE,16, EGL_NONE
    };
    EGLConfig cfg; EGLint n;
    eglChooseConfig(g_display, attr, &cfg, 1, &n);
    EGLint fmt;
    eglGetConfigAttrib(g_display, cfg, EGL_NATIVE_VISUAL_ID, &fmt);
    ANativeWindow_setBuffersGeometry(app->window, 0, 0, fmt);
    g_surface = eglCreateWindowSurface(g_display, cfg, app->window, nullptr);
    const EGLint ctx[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    g_context = eglCreateContext(g_display, cfg, EGL_NO_CONTEXT, ctx);
    eglMakeCurrent(g_display, g_surface, g_surface, g_context);
    eglQuerySurface(g_display, g_surface, EGL_WIDTH,  &g_width);
    eglQuerySurface(g_display, g_surface, EGL_HEIGHT, &g_height);
    return g_context != EGL_NO_CONTEXT;
}

static void DestroyEGL() {
    if (g_display == EGL_NO_DISPLAY) return;
    eglMakeCurrent(g_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (g_context != EGL_NO_CONTEXT) eglDestroyContext(g_display, g_context);
    if (g_surface != EGL_NO_SURFACE) eglDestroySurface(g_display, g_surface);
    eglTerminate(g_display);
    g_display=EGL_NO_DISPLAY; g_surface=EGL_NO_SURFACE; g_context=EGL_NO_CONTEXT;
}

// ── Input / Commands ──────────────────────────────────────────
static int32_t HandleInput(android_app* app, AInputEvent* ev) {
    return ImGui_ImplAndroid_HandleInputEvent(ev);
}

static void HandleCmd(android_app* app, int32_t cmd) {
    switch(cmd) {
    case APP_CMD_INIT_WINDOW:
        if (app->window && InitEGL(app)) {
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
            io.DisplaySize   = {(float)g_width, (float)g_height};
            io.FontGlobalScale = (float)g_width / 1080.f;
            ImGui_ImplAndroid_Init(app->window);
            ImGui_ImplOpenGL3_Init("#version 300 es");
            ForgeTheme::Apply();

            std::string dataPath = app->activity->internalDataPath;
            ForgeEditor_Init(dataPath, dataPath + "/icons");
            GJNI().Init(app->activity->vm, app->activity->clazz);
            g_ready = true;
            LOGI("ForgeEngine ready %dx%d", g_width, g_height);
        }
        break;
    case APP_CMD_TERM_WINDOW:
        g_ready = false;
        GJNI().Destroy();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplAndroid_Shutdown();
        ImGui::DestroyContext();
        DestroyEGL();
        break;
    case APP_CMD_WINDOW_RESIZED:
        if (g_surface != EGL_NO_SURFACE) {
            eglQuerySurface(g_display, g_surface, EGL_WIDTH,  &g_width);
            eglQuerySurface(g_display, g_surface, EGL_HEIGHT, &g_height);
            ImGui::GetIO().DisplaySize = {(float)g_width,(float)g_height};
        }
        break;
    }
}

// ── Main loop ─────────────────────────────────────────────────
void android_main(android_app* app) {
    app->onAppCmd     = HandleCmd;
    app->onInputEvent = HandleInput;

    while (!app->destroyRequested) {
        int ev; android_poll_source* src;
        while (ALooper_pollAll(0, nullptr, &ev, (void**)&src) >= 0)
            if (src) src->process(app, src);

        if (g_display == EGL_NO_DISPLAY || !g_ready) continue;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplAndroid_NewFrame();
        ImGui::NewFrame();

        ForgeEditor_Render();

        ImGui::Render();
        glViewport(0, 0, g_width, g_height);
        glClearColor(0.051f, 0.059f, 0.078f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        eglSwapBuffers(g_display, g_surface);
    }
}
