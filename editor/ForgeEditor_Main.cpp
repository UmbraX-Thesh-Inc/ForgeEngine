// ============================================================
//  ForgeEngine  –  ForgeEditor_Main.cpp
//  Updated main render loop integrating ALL new systems.
//  Replace the ForgeEditor_Render() function in ForgeEditor.cpp
// ============================================================

#include "ForgeEditor.h"
#include "../jni/JNIBridgeFull.h"
#include "panels/AllPanels.h"
#include "widgets/IconSystem.h"
#include "imgui.h"

// ════════════════════════════════════════════════════════════
//  ForgeEditorState extensions for new panels
// ════════════════════════════════════════════════════════════
// Add to GEditor().panelStates in Init:
//   "blueprint"   – Blueprint visual script editor
//   "material"    – PBR material node editor
//   "codeeditor"  – Code editor
//   "prefabs"     – Prefab library

// ════════════════════════════════════════════════════════════
//  Updated TopBar (mode tabs include Blueprint)
// ════════════════════════════════════════════════════════════
// The TopBar already handles 3D/2D. We add a blueprint mode
// button in the same row via the mode section.

// ════════════════════════════════════════════════════════════
//  Init – called once from android_main after ImGui setup
// ════════════════════════════════════════════════════════════
void ForgeEditor_Init(const std::string& projectPath,
                       const std::string& iconsPath) {
    auto& e = GEditor();

    // Panel defaults
    e.panelStates["hierarchy"].open   = true;
    e.panelStates["inspector"].open   = true;
    e.panelStates["project"].open     = true;
    e.panelStates["console"].open     = true;
    e.panelStates["animator"].open    = false;
    e.panelStates["blueprint"].open   = false;
    e.panelStates["material"].open    = false;
    e.panelStates["codeeditor"].open  = false;
    e.panelStates["prefabs"].open     = false;
    e.panelStates["network"].open     = false;

    e.projectPath = projectPath;

    // Load icon system (SVG/PNG)
    IconSystem::Get().Init(iconsPath);
    // Generate fallback SVGs if assets are missing
    IconSystem::Get().GenerateFallbackSVGs(iconsPath + "/svg");
}

// ════════════════════════════════════════════════════════════
//  Main render – called every frame
// ════════════════════════════════════════════════════════════
void ForgeEditor_Render() {
    auto& e  = GEditor();
    auto& io = ImGui::GetIO();

    // ── TOPBAR ──────────────────────────────────────────────
    RenderTopBar();

    // ── DOCKSPACE ───────────────────────────────────────────
    // Full dockspace below the top bar
    float topH = 48.f;
    ImGui::SetNextWindowPos({0, topH});
    ImGui::SetNextWindowSize({io.DisplaySize.x, io.DisplaySize.y-topH});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f,0.f});
    ImGui::Begin("##dockroot", nullptr,
        ImGuiWindowFlags_NoDecoration        |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoBringToFrontOnFocus|
        ImGuiWindowFlags_NoBackground);
    ImGui::DockSpace(ImGui::GetID("MainDock"),
                     {0,0},
                     ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();
    ImGui::PopStyleVar();

    // ── MODE-SPECIFIC FULL-SCREEN EDITORS ────────────────────
    // These take over the whole window when open
    bool fullscreen = false;

    if (e.panelStates["blueprint"].open) {
        RenderBlueprintEditor();
        fullscreen = true;
    }
    if (e.panelStates["material"].open) {
        RenderMaterialEditor();
        fullscreen = true;
    }
    if (e.panelStates["codeeditor"].open) {
        RenderCodeEditor();
        fullscreen = true;
    }
    if (e.panelStates["network"].open)    RenderNetworkPanel();

    if (!fullscreen) {
        // ── DOCKED PANELS ───────────────────────────────────
        if (e.panelStates["hierarchy"].open) RenderHierarchyPanel();
        if (e.panelStates["inspector"].open) RenderInspectorPanel();
        if (e.panelStates["project"].open)   RenderProjectPanel();
        if (e.panelStates["console"].open)   RenderConsolePanel();
        if (e.panelStates["animator"].open)  RenderAnimatorPanel();
        if (e.panelStates["prefabs"].open)   RenderPrefabPanel();

        // ── VIEWPORT ────────────────────────────────────────
        RenderViewport3D();
        if (e.mode == EditorMode::MODE_2D) RenderViewport2D();
    }

    // ── GAME PREVIEW (floating, any mode) ───────────────────
    if (e.showGamePreview) RenderGamePreview();

    // ── MODALS ──────────────────────────────────────────────
    if (e.showSettingsModal) RenderSettingsWindow();
    if (e.showBuildModal)    RenderBuildWindow();
    if (e.showNewObjPopup)   RenderNewObjectPopup();
    if (e.showAbout)         RenderAboutModal();

    // ── BOTTOM MENU BAR (Editor mode switcher) ───────────────
    // Quick-launch bar at very bottom
    float barH = 36.f;
    ImGui::SetNextWindowPos({0, io.DisplaySize.y-barH});
    ImGui::SetNextWindowSize({io.DisplaySize.x, barH});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,  ForgeTheme::BG0);
    ImGui::PushStyleColor(ImGuiCol_Border,    ForgeTheme::BORDER);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {4.f,4.f});
    ImGui::Begin("##statusbar", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoScrollbar  |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Editor quick-launch buttons
    struct QuickBtn {
        Icon        icon;
        const char* label;
        const char* key;
        ImVec4      col;
    };
    static const QuickBtn kBtns[] = {
        {Icon::GEOMETRY,      "Scene",      "hierarchy",   ForgeTheme::ACCENT  },
        {Icon::BP_FUNCTION,   "Blueprint",  "blueprint",   {0.3f,0.9f,0.5f,1.f}},
        {Icon::ASSET_MATERIAL,"Material",   "material",    {0.8f,0.4f,0.9f,1.f}},
        {Icon::CODE_FILE,     "Code",       "codeeditor",  {1.f,0.7f,0.2f,1.f}},
        {Icon::PREFAB,        "Prefabs",    "prefabs",     {0.4f,0.7f,1.f,1.f}},
        {Icon::NODE,          "Network",    "network",     {0.4f,0.9f,0.7f,1.f}},
        {Icon::SETTINGS,      "Settings",   nullptr,       ForgeTheme::TEXT1   },
    };

    for (auto& b : kBtns) {
        bool active = b.key && e.panelStates[b.key].open;
        ImVec4 tint = active ? b.col : ImVec4{b.col.x*0.5f,b.col.y*0.5f,b.col.z*0.5f,1.f};

        ImGui::PushStyleColor(ImGuiCol_Button,
            active ? ImVec4{b.col.x*0.15f,b.col.y*0.15f,b.col.z*0.15f,1.f}
                   : ForgeTheme::BG1);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            {b.col.x*0.25f,b.col.y*0.25f,b.col.z*0.25f,1.f});

        float bw = Icons().IsReady() ? 0 : 80.f;
        bool pressed;
        if (Icons().IsReady())
            pressed = Icons().ButtonLabeled(b.icon, b.label, ICON_SM);
        else
            pressed = ImGui::Button(b.label, {bw, 28});

        ImGui::PopStyleColor(2);

        if (pressed) {
            if (!b.key) {  // Settings special case
                e.showSettingsModal = true;
            } else if (b.key == std::string("blueprint") ||
                       b.key == std::string("material")  ||
                       b.key == std::string("codeeditor")) {
                // Toggle full-screen editors – close others
                bool wasOpen = e.panelStates[b.key].open;
                e.panelStates["blueprint"].open  = false;
                e.panelStates["material"].open   = false;
                e.panelStates["codeeditor"].open = false;
                if (!wasOpen) e.panelStates[b.key].open = true;
            } else {
                e.panelStates[b.key].open =
                    !e.panelStates[b.key].open;
            }
        }
        ImGui::SameLine(0, 2);
    }

    // FPS
    ImGui::SameLine(io.DisplaySize.x - 80);
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT2);
    ImGui::Text("%.0f fps", io.Framerate);
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    // ── Poll JME logs ────────────────────────────────────────
    if (GJNI().IsReady()) GJNI().PollLogs();
}
