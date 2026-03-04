// ============================================================
//  ForgeEngine  –  ProjectPanel.cpp
//  Project browser: folder tree + file grid, like Unity's
//  Project window. Sits at the bottom of the screen.
// ============================================================

#include "../ForgeEditor.h"
#include "../../jni/JNIBridgeFull.h"
#include "imgui.h"

static const char* AssetIcon(const std::string& type) {
    if (type=="model")    return u8"🧊";
    if (type=="texture")  return u8"🖼";
    if (type=="audio")    return u8"🎵";
    if (type=="material") return u8"🎨";
    if (type=="matdef")   return u8"🎨";
    if (type=="script")   return u8"📝";
    if (type=="folder")   return u8"📁";
    return u8"📄";
}
static ImVec4 AssetColor(const std::string& type) {
    if (type=="model")    return ForgeTheme::ACCENT;
    if (type=="texture")  return {0.9f,0.7f,0.2f,1.f};
    if (type=="audio")    return {0.2f,0.9f,0.5f,1.f};
    if (type=="material") return {0.7f,0.3f,1.f, 1.f};
    if (type=="script")   return {1.f, 0.7f,0.2f,1.f};
    if (type=="folder")   return ForgeTheme::ACCENT4;
    return ForgeTheme::TEXT1;
}

// ─────────────────────────────────────────────────────────────
void RenderProjectPanel() {
    auto& e     = GEditor();
    auto& state = e.panelStates["project"];
    if (!state.open) return;

    ImGuiIO& io = ImGui::GetIO();
    float panelH = state.minimized ? 28.f : 200.f;
    float panelW = io.DisplaySize.x - 230.f - 248.f; // between hierarchy & inspector
    float panelX = 230.f;
    float panelY = io.DisplaySize.y - panelH - 36.f; // above statusbar

    if (!state.floating) {
        ImGui::SetNextWindowPos({panelX, panelY}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({panelW, panelH}, ImGuiCond_Always);
    } else {
        ImGui::SetNextWindowSize({panelW, state.minimized?28.f:panelH}, ImGuiCond_Once);
    }
    ImGui::SetNextWindowSizeConstraints({300, 120}, {9999, 600});

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ForgeTheme::BG1);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f, 0.f});
    ImGui::Begin("##project", nullptr,
        ImGuiWindowFlags_NoDecoration |
        (state.floating ? 0 : ImGuiWindowFlags_NoMove) |
        ImGuiWindowFlags_NoScrollbar);

    // ── Panel header ─────────────────────────────────────────
    if (!ForgeUI::PanelHeader("##proj_hdr","Project",u8"📁", state)) {
        ImGui::End(); ImGui::PopStyleVar(); ImGui::PopStyleColor(); return;
    }
    if (state.minimized) {
        ImGui::End(); ImGui::PopStyleVar(); ImGui::PopStyleColor(); return;
    }

    // ── Toolbar ──────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ForgeTheme::BG2);
    ImGui::BeginChild("##proj_tb", {0,28}, false, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos({4,3});

    // Import button
    if (ForgeUI::SmallIconButton(u8"⬆","Import asset")) {}
    ImGui::SameLine(0,2);
    if (ForgeUI::SmallIconButton(u8"📁","New folder")) {}
    ImGui::SameLine(0,2);
    if (ForgeUI::SmallIconButton(u8"🗑","Delete selected")) {}
    ImGui::SameLine(0,8);

    // Search
    static char projSearch[64] = "";
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ForgeTheme::BG3);
    ImGui::SetNextItemWidth(150);
    ImGui::InputTextWithHint("##psrch", u8"🔍 Filter...", projSearch, 64);
    ImGui::PopStyleColor();

    // View mode
    ImGui::SameLine(0,8);
    static int viewMode = 0;  // 0=grid 1=list
    ImGui::PushStyleColor(ImGuiCol_Button,
        viewMode==0 ? ForgeTheme::BG4 : ForgeTheme::BG2);
    if (ImGui::SmallButton(u8"▦")) viewMode=0;
    ImGui::PopStyleColor();
    ImGui::SameLine(0,2);
    ImGui::PushStyleColor(ImGuiCol_Button,
        viewMode==1 ? ForgeTheme::BG4 : ForgeTheme::BG2);
    if (ImGui::SmallButton(u8"≡")) viewMode=1;
    ImGui::PopStyleColor();

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ── Body: left folder tree + right file area ──────────────
    float bodyH = ImGui::GetContentRegionAvail().y;
    float leftW = 140.f;

    // ── Left: folder tree ────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ForgeTheme::BG0);
    ImGui::BeginChild("##proj_left", {leftW, bodyH}, false);

    const char* folders[] = {
        u8"📁 Assets", u8"  📁 Models", u8"  📁 Textures",
        u8"  📁 Materials", u8"  📁 Audio", u8"  📁 Scripts",
        u8"  📁 Prefabs", u8"  📁 Scenes"
    };
    static int selFolder = 0;
    for (int i = 0; i < 8; i++) {
        bool sel = (selFolder == i);
        ImGui::PushStyleColor(ImGuiCol_Header,
            {ForgeTheme::ACCENT.x*0.15f, ForgeTheme::ACCENT.y*0.15f,
             ForgeTheme::ACCENT.z*0.15f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text,
            sel ? ForgeTheme::ACCENT : ForgeTheme::TEXT1);
        if (ImGui::Selectable(folders[i], sel, 0, {-1, 22})) selFolder = i;
        ImGui::PopStyleColor(2);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine(0,1);

    // ── Right: file grid/list ─────────────────────────────────
    // Vertical divider
    ImGui::GetWindowDrawList()->AddLine(
        {ImGui::GetCursorScreenPos().x-1, ImGui::GetCursorScreenPos().y},
        {ImGui::GetCursorScreenPos().x-1, ImGui::GetCursorScreenPos().y+bodyH},
        ImGui::ColorConvertFloat4ToU32(ForgeTheme::BORDER));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ForgeTheme::BG1);
    ImGui::BeginChild("##proj_right", {0, bodyH}, false);

    // Breadcrumb
    ImGui::SetCursorPos({4,2});
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT2);
    ImGui::TextUnformatted(u8"Assets");
    ImGui::SameLine(0,2); ImGui::TextUnformatted("›");
    ImGui::PopStyleColor();
    ImGui::SameLine(0,2);
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT0);
    ImGui::TextUnformatted("Textures");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Mock asset entries
    struct MockAsset { const char* name; const char* type; };
    static const MockAsset kAssets[] = {
        {"player.j3o",       "model"},
        {"terrain.j3o",      "model"},
        {"skybox.png",       "texture"},
        {"grass.png",        "texture"},
        {"rock_normal.png",  "texture"},
        {"bgm.ogg",          "audio"},
        {"jump.wav",         "audio"},
        {"player.j3m",       "material"},
        {"GameLogic.java",   "script"},
        {"level01.j3o",      "model"},
    };

    std::string filter(projSearch);

    if (viewMode == 0) {  // GRID
        float cellW = 68.f;
        float cellH = 76.f;
        int cols    = (int)((ImGui::GetContentRegionAvail().x) / (cellW+4));
        if (cols < 1) cols = 1;
        int col = 0;

        for (auto& a : kAssets) {
            if (!filter.empty()) {
                std::string n(a.name);
                if (n.find(filter) == std::string::npos) continue;
            }
            if (col > 0 && col % cols != 0) ImGui::SameLine(0,4);
            col++;

            ImGui::PushID(a.name);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ForgeTheme::BG3);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.f);
            ImGui::BeginChild("##ac", {cellW, cellH}, false,
                              ImGuiWindowFlags_NoScrollbar);

            // Icon (large)
            float iconSize = 32.f;
            float ix = (cellW - iconSize) * 0.5f;
            ImGui::SetCursorPos({ix, 6});
            ImGui::PushStyleColor(ImGuiCol_Text, AssetColor(a.type));
            ImGui::SetWindowFontScale(1.8f);
            ImGui::TextUnformatted(AssetIcon(a.type));
            ImGui::SetWindowFontScale(1.f);
            ImGui::PopStyleColor();

            // Name (truncated)
            std::string shortName(a.name);
            if (shortName.size() > 8) shortName = shortName.substr(0,6)+"..";
            ImGui::SetCursorPosY(44);
            float tw = ImGui::CalcTextSize(shortName.c_str()).x;
            ImGui::SetCursorPosX((cellW-tw)*0.5f);
            ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT1);
            ImGui::TextUnformatted(shortName.c_str());
            ImGui::PopStyleColor();

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", a.name);
            }

            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
    } else {  // LIST
        for (auto& a : kAssets) {
            if (!filter.empty()) {
                std::string n(a.name);
                if (n.find(filter) == std::string::npos) continue;
            }
            ImGui::PushStyleColor(ImGuiCol_Text, AssetColor(a.type));
            ImGui::TextUnformatted(AssetIcon(a.type));
            ImGui::PopStyleColor();
            ImGui::SameLine(0,6);
            ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT0);
            ImGui::Selectable(a.name, false, 0, {-80.f,20.f});
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT2);
            ImGui::TextUnformatted(a.type);
            ImGui::PopStyleColor();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}
