#pragma once
// ============================================================
//  ForgeEngine Editor  –  Dear ImGui  +  JMonkeyEngine (JNI)
//  ForgeEditor.h  –  Core editor types, state and constants
// ============================================================

#include "imgui.h"
#include "imgui_internal.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>

// ─── Forward declarations ────────────────────────────────────
class JNIBridgeFull;
struct ForgePanel;
struct ForgeWindow;

// ─── Editor colour palette ───────────────────────────────────
namespace ForgeTheme {
    // Dark slate  /  cyan accent  /  purple secondary
    constexpr ImVec4 BG0       = {0.051f, 0.059f, 0.078f, 1.f};   // #0D0F14
    constexpr ImVec4 BG1       = {0.075f, 0.086f, 0.118f, 1.f};   // #13161E
    constexpr ImVec4 BG2       = {0.102f, 0.118f, 0.157f, 1.f};   // #1A1E28
    constexpr ImVec4 BG3       = {0.129f, 0.149f, 0.204f, 1.f};   // #212635
    constexpr ImVec4 BG4       = {0.165f, 0.188f, 0.259f, 1.f};   // #2A3042
    constexpr ImVec4 ACCENT    = {0.000f, 0.831f, 1.000f, 1.f};   // #00D4FF
    constexpr ImVec4 ACCENT2   = {0.486f, 0.227f, 0.929f, 1.f};   // #7C3AED
    constexpr ImVec4 ACCENT3   = {0.063f, 0.725f, 0.506f, 1.f};   // #10B981
    constexpr ImVec4 DANGER    = {0.937f, 0.267f, 0.267f, 1.f};   // #EF4444
    constexpr ImVec4 WARNING   = {0.965f, 0.620f, 0.047f, 1.f};   // #F59E0B
    constexpr ImVec4 TEXT0     = {0.910f, 0.918f, 0.941f, 1.f};
    constexpr ImVec4 TEXT1     = {0.608f, 0.639f, 0.722f, 1.f};
    constexpr ImVec4 TEXT2     = {0.353f, 0.388f, 0.502f, 1.f};
    constexpr ImVec4 BORDER    = {1.f,    1.f,    1.f,    0.07f};
    constexpr ImVec4 BORDER_HL = {0.000f, 0.831f, 1.000f, 0.35f};
    constexpr float  ROUNDING  = 5.f;
    constexpr float  ROUNDING_SM = 3.f;

    void Apply();   // defined in ForgeEditor.cpp
}

// ─── Editor mode ─────────────────────────────────────────────
enum class EditorMode { MODE_3D, MODE_2D, BLUEPRINT };

// ─── Play state ──────────────────────────────────────────────
enum class PlayState { STOPPED, PLAYING, PAUSED };

// ─── Scene Node (mirrors JME Spatial) ────────────────────────
struct SceneNode {
    int         id          = 0;
    std::string name        = "Node";
    std::string type        = "Node";   // Node | Geometry | Light | Camera | ...
    bool        visible     = true;
    bool        expanded    = false;
    bool        selected    = false;
    int         parentId    = -1;
    std::vector<int> childIds;

    // Transform (synced via JNI)
    float translation[3] = {0,0,0};
    float rotation[3]    = {0,0,0};   // euler deg
    float scale[3]       = {1,1,1};
};

// ─── Asset entry ─────────────────────────────────────────────
struct AssetEntry {
    std::string name;
    std::string path;
    std::string type;   // folder | j3o | png | wav | mat | ...
    bool        isDir   = false;
    std::vector<std::shared_ptr<AssetEntry>> children;
};

// ─── Console log ─────────────────────────────────────────────
enum class LogLevel { INFO, WARNING, ERR, JME };
struct LogEntry {
    LogLevel    level;
    std::string msg;
    std::string time;
};

// ─── Panel flags (mirrors ImGuiWindowFlags) ───────────────────
struct PanelState {
    bool open      = true;
    bool minimized = false;
    bool floating  = false;
    ImVec2 savedSize;
};

// ─── Animation clip ──────────────────────────────────────────
struct AnimClip {
    std::string name;
    float       length  = 1.f;   // seconds
    float       fps     = 30.f;
    bool        loop    = true;
    bool        playing = false;
    float       cursor  = 0.f;
};

// ─── Global editor state ──────────────────────────────────────
struct ForgeEditorState {
    // Mode
    EditorMode  mode      = EditorMode::MODE_3D;
    PlayState   playState = PlayState::STOPPED;

    // Scene
    std::vector<SceneNode>              sceneNodes;
    int                                 selectedNodeId = -1;
    std::string                         sceneName      = "Untitled";
    bool                                sceneDirty     = false;

    // Assets
    std::shared_ptr<AssetEntry>         assetRoot;
    std::string                         currentAssetPath = "/assets";

    // Console
    std::vector<LogEntry>               logs;
    bool                                consoleAutoScroll = true;

    // Animation
    std::vector<AnimClip>               animClips;
    int                                 selectedClip = -1;

    // Panel open/close/min
    std::unordered_map<std::string, PanelState> panelStates;

    // Viewport
    bool        showGamePreview   = false;
    ImVec2      gamePreviewSize   = {400, 300};
    bool        viewportGizmos    = true;
    bool        viewportGrid      = true;

    // Project
    std::string projectName       = "MyGame";
    std::string projectPath       = "/sdcard/ForgeProjects/MyGame";

    // Toolbar toggles
    bool        snapEnabled       = false;
    float       snapValue         = 0.25f;
    int         gizmoMode         = 0;   // 0=translate 1=rotate 2=scale
    int         coordSpace        = 0;   // 0=local 1=world

    // UI helpers
    bool        showMenuBar       = false;
    bool        showSettingsModal = false;
    bool        showBuildModal    = false;
    bool        showNewObjPopup   = false;
    bool        showAbout         = false;
};

// ─── Editor lifecycle (called from android_main) ─────────────
void ForgeEditor_Init(const std::string& projectPath,
                       const std::string& iconsPath);
void ForgeEditor_Render();

// ─── Singleton accessor ───────────────────────────────────────
ForgeEditorState& GEditor();
JNIBridgeFull&    GJNI();

// ─── Utility helpers ──────────────────────────────────────────
namespace ForgeUI {
    // Styled button – returns true on press
    bool IconButton(const char* icon, const char* tooltip = nullptr,
                    ImVec4 col = ForgeTheme::ACCENT, float size = 28.f);
    bool SmallIconButton(const char* icon, const char* tooltip = nullptr);
    void Separator();
    void SectionHeader(const char* label);
    bool PanelHeader(const char* id, const char* label, const char* icon,
                     PanelState& state);
    void TooltipIfHovered(const char* tip);
    void Badge(const char* text, ImVec4 col);
    // Mobile-friendly drag float
    bool DragFloat3Compact(const char* id, float* v, float speed = 0.1f);
    bool DragFloat3CompactLabeled(const char* label, float* v, float speed=0.1f);
}
