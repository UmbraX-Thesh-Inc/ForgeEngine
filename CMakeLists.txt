cmake_minimum_required(VERSION 3.22)
project(ForgeEngine VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ROOT_DIR       ${CMAKE_SOURCE_DIR})
set(EDITOR_DIR     ${ROOT_DIR}/editor)
set(JNI_DIR        ${ROOT_DIR}/jni)
set(IMGUI_DIR      ${ROOT_DIR}/third_party/imgui)
set(NANOSVG_DIR    ${ROOT_DIR}/third_party/nanosvg)
set(STB_DIR        ${ROOT_DIR}/third_party/stb)
set(GLUE_DIR       ${ROOT_DIR}/third_party/android_native_app_glue)

# android_native_app_glue
add_library(android_native_app_glue STATIC
    ${GLUE_DIR}/android_native_app_glue.c)
target_include_directories(android_native_app_glue PUBLIC ${GLUE_DIR})

# Dear ImGui
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_android.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# ForgeEngine sources
set(FORGE_SOURCES
    # Entry point
    ${ROOT_DIR}/android/android_main.cpp

    # JNI bridge (full, replaces legacy)
    ${JNI_DIR}/JNIBridgeFull.cpp

    # Editor core
    ${EDITOR_DIR}/ForgeEditor.cpp
    ${EDITOR_DIR}/ForgeEditor_Main.cpp

    # Panels (all panels + windows + viewport in one file)
    ${EDITOR_DIR}/panels/TopBar.cpp
    ${EDITOR_DIR}/panels/HierarchyPanel.cpp
    ${EDITOR_DIR}/panels/InspectorPanel.cpp
    ${EDITOR_DIR}/panels/InspectorPanelFull.cpp
    ${EDITOR_DIR}/panels/ProjectPanel.cpp
    ${EDITOR_DIR}/panels/EditorPanels.cpp   # Console, Animator, Viewport, GamePreview, Settings, Build, About

    # Widgets
    ${EDITOR_DIR}/widgets/IconSystem.cpp

    # Full-screen editors
    ${EDITOR_DIR}/blueprint/BlueprintEditor.cpp
    ${EDITOR_DIR}/material/MaterialEditor.cpp
    ${EDITOR_DIR}/gizmo/TransformGizmo.cpp
    ${EDITOR_DIR}/prefab/PrefabSystem.cpp
    ${EDITOR_DIR}/codeeditor/CodeEditor.cpp
    ${EDITOR_DIR}/network/NetworkPanel.cpp
)

add_library(forge_editor SHARED ${IMGUI_SOURCES} ${FORGE_SOURCES})

target_include_directories(forge_editor PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${NANOSVG_DIR}
    ${STB_DIR}
    ${GLUE_DIR}
    ${ROOT_DIR}
    ${ROOT_DIR}/android
    ${EDITOR_DIR}
    ${EDITOR_DIR}/widgets
    ${EDITOR_DIR}/panels
    ${EDITOR_DIR}/windows
    ${EDITOR_DIR}/prefab
    ${JNI_DIR}
)

find_library(log-lib     log)
find_library(android-lib android)
find_library(egl-lib     EGL)
find_library(gles3-lib   GLESv3)
find_library(jnigfx-lib  jnigraphics)

target_link_libraries(forge_editor
    android_native_app_glue
    ${android-lib}
    ${log-lib}
    ${egl-lib}
    ${gles3-lib}
    ${jnigfx-lib}
)

target_compile_options(forge_editor PRIVATE
    -Wall
    -Wno-unused-parameter
    -O2
    -ffast-math
    -fvisibility=hidden
    -DANDROID
    -DIMGUI_IMPL_OPENGL_ES3
)

message(STATUS "ForgeEngine build: ABI=${ANDROID_ABI}, API=${ANDROID_PLATFORM}")

# ── Setup notes ───────────────────────────────────────────────
# mkdir -p third_party
# git clone --depth 1 https://github.com/ocornut/imgui third_party/imgui
# git clone --depth 1 https://github.com/memononen/nanosvg third_party/nanosvg
# cp third_party/nanosvg/src/nanosvg.h    third_party/nanosvg/
# cp third_party/nanosvg/src/nanosvgrast.h third_party/nanosvg/
# mkdir third_party/stb
# wget -P third_party/stb https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
# cp -r $NDK/sources/android/native_app_glue third_party/android_native_app_glue
