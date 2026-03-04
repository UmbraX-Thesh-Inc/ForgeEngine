# ⚡ ForgeEngine Editor

Mobile-first 3D/2D game editor built on **JMonkeyEngine** + **Dear ImGui**,
running natively on Android via the NDK.

---

## Architecture

```
┌─────────────────────────────────────────────────────┐
│              Android (NDK / OpenGL ES 3)             │
│                                                     │
│  ┌──────────────────────────────────────────────┐   │
│  │           Dear ImGui  (C++)                  │   │
│  │                                              │   │
│  │  TopBar  Hierarchy  Inspector  Project       │   │
│  │  Console  Animator  Viewport  GamePreview    │   │
│  │  Settings  Build  NewObject  About           │   │
│  └──────────────┬───────────────────────────────┘   │
│                 │  ForgeEditorState (shared)         │
│  ┌──────────────▼───────────────────────────────┐   │
│  │           JNI Bridge (C++)                   │   │
│  │  JNIBridge.h / JNIBridge.cpp                 │   │
│  └──────────────┬───────────────────────────────┘   │
│                 │  JNI calls                         │
│  ┌──────────────▼───────────────────────────────┐   │
│  │     JMonkeyEngine (Java / Kotlin)             │   │
│  │  EditorBridge.java  ForgeJMEApp.java          │   │
│  │  ForgeCommandStack.java                       │   │
│  └──────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

---

## Editor Features

| Panel / Window        | Description                                        |
|-----------------------|----------------------------------------------------|
| **TopBar**            | ☰ Menu · 💾 Save · 3D/2D toggle · ▶⏸⏹ Play controls · Gizmo bar |
| **Hierarchy**         | Scene tree, + add objects popup, visibility, context menu |
| **Inspector**         | Transform (T/R/S) · Material · Rigid Body · Script · Add Component |
| **Project**           | Folder tree + file grid/list browser               |
| **Console**           | Level-filtered log (Info/Warn/Error/JME), autoscroll |
| **Animator**          | Clip list, timeline ruler, keyframe scrubber       |
| **Viewport 3D**       | JME render texture + grid overlay + gizmo hints    |
| **Viewport 2D**       | Orthographic UI editor (Godot-style)               |
| **Game Preview**      | Floating resizable play window                     |
| **Settings**          | Editor / Rendering / Input tabs                    |
| **Build**             | Android APK / Desktop build with progress bar      |

All panels support: **open · close · minimize · float/dock**.

---

## JNI Bridge API (C++ → Java)

```cpp
// Engine control
GJNI().StartGame();
GJNI().PauseGame();
GJNI().StopGame();

// Scene
GJNI().NewScene("Level1");
GJNI().SaveScene("/sdcard/Projects/MyGame/scenes/level1.j3o");
GJNI().LoadScene(path);

// Objects
int id = GJNI().AddSpatial(JMESpatialType::BOX, "Box", parentId);
GJNI().SetTranslation(id, x, y, z);
GJNI().SetRotation(id, ex, ey, ez);   // euler degrees
GJNI().SetScale(id, sx, sy, sz);

// Animation
GJNI().PlayAnimation(id, "Walk", true);
GJNI().SetAnimSpeed(id, 1.5f);

// Build
GJNI().BuildProject("/sdcard/Builds/", "android", progressCallback);

// Undo / Redo
GJNI().Undo();
GJNI().Redo();
```

---

## Build Instructions

### Prerequisites
- Android NDK r25+
- Android SDK API 26+
- CMake 3.22+
- Java 11+
- JMonkeyEngine 3.x SDK (place JARs in `android/libs/`)
- Dear ImGui (clone into `third_party/imgui/`)

### Steps

```bash
# 1. Clone ImGui
mkdir -p third_party
git clone https://github.com/ocornut/imgui third_party/imgui

# 2. Copy android_native_app_glue from NDK
cp $NDK/sources/android/native_app_glue \
   third_party/android_native_app_glue -r

# 3. Configure with CMake (Android toolchain)
mkdir build && cd build
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_BUILD_TYPE=Release

# 4. Build
make -j$(nproc)

# 5. Package with Gradle (Android project wrapping the .so)
cd ../android_app
./gradlew assembleDebug
```

---

## Planned: Blueprint Editor

The `EditorMode::BLUEPRINT` mode will provide a visual scripting
graph editor (nodes + pins + wires) inspired by Unreal Engine blueprints.
The graph will compile to JVM bytecode via a custom code generator
and inject into the JME scene at runtime.

---

## File Structure

```
ForgeEngine/
├── CMakeLists.txt
├── editor/
│   ├── ForgeEditor.h          ← Core types, state, theme, UI helpers
│   ├── ForgeEditor.cpp        ← Init, theme, main render loop
│   └── panels/
│       ├── HierarchyPanel.h   ← Forward declarations
│       ├── TopBar.cpp
│       ├── HierarchyPanel.cpp
│       ├── InspectorPanel.cpp
│       ├── ProjectPanel.cpp
│       └── EditorPanels.cpp   ← Console, Animator, Viewports,
│                                 GamePreview, Settings, Build, About
├── jni/
│   ├── JNIBridge.h
│   └── JNIBridge.cpp
└── android/
    ├── android_main.cpp
    ├── EditorBridge.java
    ├── ForgeJMEApp.java
    └── ForgeCommandStack.java
```
