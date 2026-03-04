// ============================================================
//  ForgeEngine  –  TopBar.cpp
//  Top bar: hamburger menu | save | 2D/3D toggle |
//           play / pause / stop | snap / gizmo controls
// ============================================================

#include "../ForgeEditor.h"
#include "../../jni/JNIBridgeFull.h"
#include "imgui.h"

// ─────────────────────────────────────────────────────────────
static void DrawMenuDropdown();
static void DrawGizmoBar();

void RenderTopBar() {
    auto& e = GEditor();
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({io.DisplaySize.x, 48.f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {6.f, 6.f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ForgeTheme::BG1);
    ImGui::PushStyleColor(ImGuiCol_Border,   ForgeTheme::BORDER);
    ImGui::Begin("##topbar", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoScrollbar  |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    float y = (48.f - ImGui::GetFrameHeight()) * 0.5f;
    ImGui::SetCursorPosY(y);

    // ── ☰  Hamburger menu ────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Button,        {0,0,0,0});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {ForgeTheme::ACCENT.x*0.15f,
                                                   ForgeTheme::ACCENT.y*0.15f,
                                                   ForgeTheme::ACCENT.z*0.15f,1.f});
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT);
    if (ImGui::Button(u8"☰", {36, 36})) e.showMenuBar = !e.showMenuBar;
    ImGui::PopStyleColor(3);
    ForgeUI::TooltipIfHovered("Menu");

    // ── Project name ─────────────────────────────────────────
    ImGui::SameLine(0, 4);
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT1);
    ImGui::SetCursorPosY(y + 4);
    ImGui::Text("%s", e.projectName.c_str());
    ImGui::PopStyleColor();
    if (e.sceneDirty) {
        ImGui::SameLine(0,2);
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::WARNING);
        ImGui::TextUnformatted("●");
        ImGui::PopStyleColor();
    }

    // ── Save button ──────────────────────────────────────────
    ImGui::SameLine(0, 10);
    ImGui::SetCursorPosY(y);
    if (ForgeUI::IconButton(u8"💾", "Save Scene  (Ctrl+S)",
                            ForgeTheme::ACCENT3, 36)) {
        GJNI().SaveScene(e.projectPath + "/" + e.sceneName + ".j3o");
        e.sceneDirty = false;
    }

    // ── 3D / 2D mode toggle ──────────────────────────────────
    ImGui::SameLine(0, 8);
    ImGui::SetCursorPosY(y);
    {
        bool is3D = (e.mode == EditorMode::MODE_3D);
        ImGui::PushStyleColor(ImGuiCol_Button,
            is3D ? ForgeTheme::BG4 : ForgeTheme::BG2);
        ImGui::PushStyleColor(ImGuiCol_Text,
            is3D ? ForgeTheme::ACCENT : ForgeTheme::TEXT2);
        if (ImGui::Button("3D##mode", {34,34})) {
            e.mode = EditorMode::MODE_3D;
            GJNI().SetEditorMode(true);
        }
        ImGui::PopStyleColor(2);
        ForgeUI::TooltipIfHovered("3D Editor");

        ImGui::SameLine(0,2);
        ImGui::PushStyleColor(ImGuiCol_Button,
            !is3D ? ForgeTheme::BG4 : ForgeTheme::BG2);
        ImGui::PushStyleColor(ImGuiCol_Text,
            !is3D ? ForgeTheme::ACCENT : ForgeTheme::TEXT2);
        if (ImGui::Button("2D##mode", {34,34})) {
            e.mode = EditorMode::MODE_2D;
            GJNI().SetEditorMode(false);
        }
        ImGui::PopStyleColor(2);
        ForgeUI::TooltipIfHovered("2D / UI Editor");
    }

    // ── CENTER: Play / Pause / Stop ──────────────────────────
    float pbw  = 36.f * 3 + 8.f;
    float cx   = (io.DisplaySize.x - pbw) * 0.5f;
    ImGui::SameLine();
    ImGui::SetCursorPosX(cx);
    ImGui::SetCursorPosY(y);

    bool playing = (e.playState == PlayState::PLAYING);
    bool paused  = (e.playState == PlayState::PAUSED);
    bool stopped = (e.playState == PlayState::STOPPED);

    // Play
    ImGui::PushStyleColor(ImGuiCol_Text,
        playing ? ForgeTheme::ACCENT3 : ForgeTheme::TEXT0);
    if (ForgeUI::IconButton(u8"▶", "Play  (F5)",
                            playing ? ForgeTheme::ACCENT3 : ForgeTheme::TEXT1, 36)) {
        if (stopped || paused) {
            e.playState      = PlayState::PLAYING;
            e.showGamePreview= true;
            GJNI().StartGame();
        }
    }
    ImGui::PopStyleColor();

    ImGui::SameLine(0,4);

    // Pause
    ImGui::PushStyleColor(ImGuiCol_Text,
        paused ? ForgeTheme::WARNING : ForgeTheme::TEXT0);
    if (ForgeUI::IconButton(u8"⏸", "Pause  (F6)",
                            paused ? ForgeTheme::WARNING : ForgeTheme::TEXT1, 36)) {
        if (playing) {
            e.playState = PlayState::PAUSED;
            GJNI().PauseGame();
        }
    }
    ImGui::PopStyleColor();

    ImGui::SameLine(0,4);

    // Stop
    ImGui::PushStyleColor(ImGuiCol_Text,
        stopped ? ForgeTheme::TEXT2 : ForgeTheme::DANGER);
    if (ForgeUI::IconButton(u8"⏹", "Stop  (F7)",
                            stopped ? ForgeTheme::TEXT2 : ForgeTheme::DANGER, 36)) {
        if (!stopped) {
            e.playState      = PlayState::STOPPED;
            e.showGamePreview= false;
            GJNI().StopGame();
        }
    }
    ImGui::PopStyleColor();

    // ── RIGHT: Gizmo / Snap / Undo ───────────────────────────
    float rstart = io.DisplaySize.x - 180.f;
    ImGui::SameLine();
    ImGui::SetCursorPosX(rstart);
    ImGui::SetCursorPosY(y);
    DrawGizmoBar();

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    // ── Dropdown menu ─────────────────────────────────────────
    if (e.showMenuBar) DrawMenuDropdown();
}

// ─────────────────────────────────────────────────────────────
static void DrawGizmoBar() {
    auto& e = GEditor();
    const char* gizmoIcons[3] = {u8"↔", u8"↻", u8"⇲"};
    const char* gizmoTips[3]  = {"Translate (W)", "Rotate (E)", "Scale (R)"};

    for (int i = 0; i < 3; i++) {
        ImGui::PushStyleColor(ImGuiCol_Button,
            i == e.gizmoMode ? ForgeTheme::BG4 : ForgeTheme::BG2);
        ImGui::PushStyleColor(ImGuiCol_Text,
            i == e.gizmoMode ? ForgeTheme::ACCENT : ForgeTheme::TEXT1);
        if (ImGui::Button(gizmoIcons[i], {30,34})) e.gizmoMode = i;
        ImGui::PopStyleColor(2);
        ForgeUI::TooltipIfHovered(gizmoTips[i]);
        if (i < 2) ImGui::SameLine(0,2);
    }

    ImGui::SameLine(0, 8);

    // Snap
    ImGui::PushStyleColor(ImGuiCol_Button,
        e.snapEnabled ? ForgeTheme::BG4 : ForgeTheme::BG2);
    ImGui::PushStyleColor(ImGuiCol_Text,
        e.snapEnabled ? ForgeTheme::ACCENT4 : ForgeTheme::TEXT2);
    if (ImGui::Button(u8"⊞", {30,34})) e.snapEnabled = !e.snapEnabled;
    ImGui::PopStyleColor(2);
    ForgeUI::TooltipIfHovered(e.snapEnabled ? "Snap ON" : "Snap OFF");

    ImGui::SameLine(0,8);

    // Undo / Redo
    if (ForgeUI::SmallIconButton(u8"↩", "Undo  (Ctrl+Z)")) GJNI().Undo();
    ImGui::SameLine(0,2);
    if (ForgeUI::SmallIconButton(u8"↪", "Redo  (Ctrl+Y)")) GJNI().Redo();
}

// ─────────────────────────────────────────────────────────────
static void DrawMenuDropdown() {
    auto& e = GEditor();
    ImGui::SetNextWindowPos({4, 50});
    ImGui::SetNextWindowSize({220, 0});
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ForgeTheme::BG2);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8.f, 8.f});

    if (ImGui::Begin("##menu_drop", &e.showMenuBar,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove       |
            ImGuiWindowFlags_NoBringToFrontOnFocus)) {

        auto MenuItem = [&](const char* icon, const char* label,
                            const char* shortcut = nullptr) -> bool {
            ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT);
            ImGui::TextUnformatted(icon);
            ImGui::PopStyleColor();
            ImGui::SameLine(28);
            bool pressed = ImGui::Selectable(label, false, 0, {150,0});
            if (shortcut) {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT2);
                ImGui::Text("%s", shortcut);
                ImGui::PopStyleColor();
            }
            return pressed;
        };

        ForgeUI::SectionHeader("  PROJECT");
        if (MenuItem(u8"🗋", "New Scene",   "Ctrl+N")) {
            GJNI().NewScene("Untitled");
            e.sceneDirty = false;
            e.showMenuBar = false;
        }
        if (MenuItem(u8"📂", "Open Scene",  "Ctrl+O")) { e.showMenuBar=false; /* TODO: file picker */ }
        if (MenuItem(u8"💾", "Save Scene",  "Ctrl+S")) {
            GJNI().SaveScene(e.projectPath+"/"+e.sceneName+".j3o");
            e.sceneDirty=false; e.showMenuBar=false;
        }
        if (MenuItem(u8"📋", "Save As..."))  { e.showMenuBar=false; }

        ImGui::Spacing();
        ForgeUI::SectionHeader("  EDITOR");
        if (MenuItem(u8"⚙", "Settings"))     { e.showSettingsModal=true; e.showMenuBar=false; }
        if (MenuItem(u8"🔨","Build Project")) { e.showBuildModal=true;    e.showMenuBar=false; }
        if (MenuItem(u8"📊","Project Stats")) { e.showMenuBar=false; }

        ImGui::Spacing();
        ForgeUI::SectionHeader("  PANELS");
        auto PanelToggle = [&](const char* icon, const char* name, const char* key) {
            bool& open = e.panelStates[key].open;
            ImGui::PushStyleColor(ImGuiCol_Text, open ? ForgeTheme::ACCENT : ForgeTheme::TEXT2);
            ImGui::TextUnformatted(icon);
            ImGui::PopStyleColor();
            ImGui::SameLine(28);
            if (ImGui::Selectable(name, open, 0, {150,0})) { open = !open; e.showMenuBar=false; }
        };
        PanelToggle(u8"🌲","Hierarchy",  "hierarchy");
        PanelToggle(u8"🔍","Inspector",  "inspector");
        PanelToggle(u8"📁","Project",    "project");
        PanelToggle(u8">_","Console",    "console");
        PanelToggle(u8"🎞","Animator",   "animator");

        ImGui::Spacing();
        ForgeUI::SectionHeader("  ABOUT");
        if (MenuItem(u8"ℹ","About ForgeEngine")) { e.showAbout=true; e.showMenuBar=false; }

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::DANGER);
        if (ImGui::Selectable(u8"⏏  Exit to Projects", false, 0, {160,0})) {
            // Return to project list activity
            // mActivity.startActivity(ProjectListActivity.class)
            e.showMenuBar = false;
        }
        ImGui::PopStyleColor();
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    // Click outside to close
    if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) &&
        ImGui::IsMouseClicked(0))
        e.showMenuBar = false;
}
