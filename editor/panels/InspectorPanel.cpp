// ============================================================
//  ForgeEngine  –  InspectorPanel.cpp
//  Inspector: Transform, Material, custom components,
//  add-component button, all mobile-friendly
// ============================================================

#include "../ForgeEditor.h"
#include "../../jni/JNIBridgeFull.h"
#include "imgui.h"

// ─────────────────────────────────────────────────────────────
static SceneNode* SelectedNode() {
    auto& e = GEditor();
    if (e.selectedNodeId < 0) return nullptr;
    for (auto& n : e.sceneNodes)
        if (n.id == e.selectedNodeId) return &n;
    return nullptr;
}

static bool ComponentHeader(const char* icon, const char* label,
                             bool& open, bool removable=true) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ForgeTheme::BG3);
    ImGui::BeginChild(("##ch_"+std::string(label)).c_str(),
                      {0, 28}, false, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos({4, 5});
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT);
    ImGui::TextUnformatted(icon);
    ImGui::PopStyleColor();
    ImGui::SameLine(0,5);
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT0);
    bool toggled = ImGui::Selectable(label, false, 0, {-50.f, 0});
    if (toggled) open = !open;
    ImGui::PopStyleColor();

    float bx = ImGui::GetWindowWidth() - 46.f;
    ImGui::SameLine(bx);
    ImGui::PushStyleColor(ImGuiCol_Text, open ? ForgeTheme::ACCENT : ForgeTheme::TEXT2);
    ImGui::TextUnformatted(open ? u8"▾" : u8"▸");
    ImGui::PopStyleColor();
    if (removable) {
        ImGui::SameLine(0,4);
        if (ForgeUI::SmallIconButton(u8"✕","Remove component")) {}
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    return open;
}

// ─────────────────────────────────────────────────────────────
void RenderInspectorPanel() {
    auto& e     = GEditor();
    auto& state = e.panelStates["inspector"];
    if (!state.open) return;

    ImGuiIO& io = ImGui::GetIO();
    float panelW = 248.f;
    float panelH = io.DisplaySize.y - 48.f - 36.f;

    if (!state.floating) {
        ImGui::SetNextWindowPos({io.DisplaySize.x - panelW, 48.f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({panelW, panelH}, ImGuiCond_Always);
    } else {
        ImGui::SetNextWindowSize({panelW, panelH}, ImGuiCond_Once);
    }
    ImGui::SetNextWindowSizeConstraints({180, 300}, {600, 3000});

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ForgeTheme::BG1);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f, 0.f});
    ImGui::Begin("##inspector", nullptr,
        ImGuiWindowFlags_NoDecoration |
        (state.floating ? 0 : ImGuiWindowFlags_NoMove) |
        ImGuiWindowFlags_NoScrollbar);

    // ── Panel header ─────────────────────────────────────────
    if (!ForgeUI::PanelHeader("##insp_hdr","Inspector",u8"🔍", state)) {
        ImGui::End(); ImGui::PopStyleVar(); ImGui::PopStyleColor(); return;
    }

    SceneNode* node = SelectedNode();
    if (!node) {
        ImGui::SetCursorPosY(80);
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT2);
        float tw = ImGui::CalcTextSize("Select an object").x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth()-tw)*0.5f);
        ImGui::TextUnformatted("Select an object");
        ImGui::PopStyleColor();
        ImGui::End(); ImGui::PopStyleVar(); ImGui::PopStyleColor(); return;
    }

    ImGui::BeginChild("##insp_scroll", {0,0}, false);
    ImGui::SetCursorPos({6,4});

    // ── Name + Active toggle ──────────────────────────────────
    static char nameBuf[128];
    strncpy(nameBuf, node->name.c_str(), 127);
    nameBuf[127]='\0';

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ForgeTheme::BG3);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 36);
    if (ImGui::InputText("##name", nameBuf, 128)) node->name = nameBuf;
    ImGui::PopStyleColor();
    ImGui::SameLine(0,4);
    ImGui::PushStyleColor(ImGuiCol_Text,
        node->visible ? ForgeTheme::ACCENT3 : ForgeTheme::TEXT2);
    if (ImGui::Checkbox("##vis", &node->visible)) {}
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Type badge
    ImGui::SetCursorPosX(6);
    ForgeUI::Badge(node->type.c_str(),
        {ForgeTheme::ACCENT2.x*0.4f,
         ForgeTheme::ACCENT2.y*0.4f,
         ForgeTheme::ACCENT2.z*0.4f, 1.f});
    ImGui::Spacing();
    ForgeUI::Separator();
    ImGui::Spacing();

    // ═════════════════════════════════════════════════════════
    //  TRANSFORM
    // ═════════════════════════════════════════════════════════
    static bool xformOpen = true;
    if (ComponentHeader(u8"↔","Transform", xformOpen, false)) {
        ImGui::SetCursorPosX(6);
        ImGui::Spacing();
        bool changed = false;
        changed |= ForgeUI::DragFloat3CompactLabeled("Position", node->translation, 0.1f);
        ImGui::Spacing();
        changed |= ForgeUI::DragFloat3CompactLabeled("Rotation", node->rotation, 0.5f);
        ImGui::Spacing();
        changed |= ForgeUI::DragFloat3CompactLabeled("Scale",    node->scale,    0.01f);
        ImGui::Spacing();

        if (changed) {
            GJNI().SetTranslation(node->id,
                node->translation[0], node->translation[1], node->translation[2]);
            GJNI().SetRotation(node->id,
                node->rotation[0], node->rotation[1], node->rotation[2]);
            GJNI().SetScale(node->id,
                node->scale[0], node->scale[1], node->scale[2]);
        }

        // Reset button
        ImGui::SetCursorPosX(6);
        if (ImGui::SmallButton(u8"⟳ Reset Transform")) {
            node->translation[0]=0; node->translation[1]=0; node->translation[2]=0;
            node->rotation[0]=0;    node->rotation[1]=0;    node->rotation[2]=0;
            node->scale[0]=1;       node->scale[1]=1;       node->scale[2]=1;
            GJNI().SetTranslation(node->id, 0,0,0);
            GJNI().SetRotation(node->id, 0,0,0);
            GJNI().SetScale(node->id, 1,1,1);
        }
        ImGui::Spacing();
    }
    ImGui::Spacing();

    // ═════════════════════════════════════════════════════════
    //  MATERIAL  (only for Geometry)
    // ═════════════════════════════════════════════════════════
    if (node->type == "Geometry") {
        static bool matOpen = true;
        if (ComponentHeader(u8"🎨","Material", matOpen)) {
            ImGui::SetCursorPosX(6);
            ImGui::Spacing();
            static float albedo[4] = {0.5f,0.5f,0.5f,1.f};
            static float metallic  = 0.f;
            static float roughness = 0.5f;
            static float emissive[3] = {0,0,0};

            ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT1);
            ImGui::TextUnformatted("Albedo");    ImGui::PopStyleColor();
            ImGui::SameLine(80);
            ImGui::SetNextItemWidth(-1);
            ImGui::ColorEdit4("##alb", albedo, ImGuiColorEditFlags_NoLabel);

            ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT1);
            ImGui::TextUnformatted("Metallic");  ImGui::PopStyleColor();
            ImGui::SameLine(80); ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##met", &metallic,  0.f, 1.f);

            ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT1);
            ImGui::TextUnformatted("Roughness"); ImGui::PopStyleColor();
            ImGui::SameLine(80); ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##rou", &roughness, 0.f, 1.f);

            ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT1);
            ImGui::TextUnformatted("Emissive");  ImGui::PopStyleColor();
            ImGui::SameLine(80); ImGui::SetNextItemWidth(-1);
            ImGui::ColorEdit3("##emi", emissive, ImGuiColorEditFlags_NoLabel);

            ImGui::Spacing();
            // Texture slots
            ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT2);
            ImGui::TextUnformatted("Textures");  ImGui::PopStyleColor();
            ImGui::Separator();
            const char* slots[] = {"Diffuse","Normal","Specular","AO"};
            for (auto& s : slots) {
                ImGui::PushStyleColor(ImGuiCol_Button, ForgeTheme::BG3);
                ImGui::PushStyleColor(ImGuiCol_Text,   ForgeTheme::TEXT1);
                ImGui::Button((std::string(s)+" (drag here)").c_str(), {-1,24});
                ImGui::PopStyleColor(2);
            }
            ImGui::Spacing();
        }
        ImGui::Spacing();
    }

    // ═════════════════════════════════════════════════════════
    //  RIGID BODY (if applicable)
    // ═════════════════════════════════════════════════════════
    static bool rbOpen = false;
    if (ComponentHeader(u8"⚙","Rigid Body", rbOpen)) {
        ImGui::Spacing();
        static float mass = 1.f;
        static bool  kinematic = false;
        static bool  trigger   = false;
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT1);
        ImGui::TextUnformatted("Mass");       ImGui::PopStyleColor();
        ImGui::SameLine(80); ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##mass", &mass, 0.1f, 0.f, 9999.f);
        ImGui::Checkbox("Kinematic##rb", &kinematic);
        ImGui::Checkbox("Is Trigger##rb", &trigger);
        ImGui::Spacing();
    }
    ImGui::Spacing();

    // ═════════════════════════════════════════════════════════
    //  SCRIPT
    // ═════════════════════════════════════════════════════════
    static bool scriptOpen = false;
    if (ComponentHeader(u8"📝","Script", scriptOpen)) {
        ImGui::Spacing();
        static char scriptName[64] = "MyScript";
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ForgeTheme::BG3);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##scr", scriptName, 64);
        ImGui::PopStyleColor();
        ImGui::Spacing();
        if (ImGui::SmallButton(u8"📝 Edit Script")) {}
        ImGui::Spacing();
    }
    ImGui::Spacing();

    // ═════════════════════════════════════════════════════════
    //  ADD COMPONENT button
    // ═════════════════════════════════════════════════════════
    float bx = (ImGui::GetContentRegionAvail().x - 180.f) * 0.5f;
    ImGui::SetCursorPosX(bx);
    ImGui::PushStyleColor(ImGuiCol_Button,
        {ForgeTheme::ACCENT.x*0.15f, ForgeTheme::ACCENT.y*0.15f,
         ForgeTheme::ACCENT.z*0.15f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        {ForgeTheme::ACCENT.x*0.3f, ForgeTheme::ACCENT.y*0.3f,
         ForgeTheme::ACCENT.z*0.3f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT);
    ImGui::Button(u8"＋  Add Component", {180, 32});
    ImGui::PopStyleColor(3);

    if (ImGui::BeginPopupContextItem("##addcomp",
            ImGuiPopupFlags_MouseButtonLeft)) {
        const char* comps[] = {
            u8"⚙  Rigid Body",   u8"🎵  Audio Source",
            u8"🎞  Animator",     u8"📝  Script",
            u8"💡  Light",        u8"🎥  Camera",
            u8"🌫  Particle Sys", u8"🖼  Canvas (UI)"
        };
        for (auto& c : comps)
            if (ImGui::MenuItem(c)) {}
        ImGui::EndPopup();
    }
    ImGui::Spacing(); ImGui::Spacing();

    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}
