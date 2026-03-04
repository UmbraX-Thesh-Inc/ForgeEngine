// ============================================================
//  ForgeEngine  –  HierarchyPanel.cpp
//  Scene hierarchy: tree view, add (+), select, delete,
//  visibility toggle, drag-to-reparent
// ============================================================

#include "../ForgeEditor.h"
#include "../../jni/JNIBridgeFull.h"
#include "imgui.h"
#include <algorithm>

// ─── Object type catalogue ───────────────────────────────────
struct ObjType {
    const char* icon;
    const char* name;
    JMESpatialType type;
};
static const ObjType kObjTypes[] = {
    {u8"📦","Node (Empty)",      JMESpatialType::NODE},
    {u8"🧊","Box",               JMESpatialType::BOX},
    {u8"🔮","Sphere",            JMESpatialType::SPHERE},
    {u8"🥫","Cylinder",          JMESpatialType::CYLINDER},
    {u8"💊","Capsule",           JMESpatialType::CAPSULE},
    {u8"🍩","Torus",             JMESpatialType::TORUS},
    {u8"▬", "Plane",            JMESpatialType::PLANE},
    {u8"🗺","Terrain",           JMESpatialType::TERRAIN},
    {u8"💡","Point Light",       JMESpatialType::LIGHT_POINT},
    {u8"☀","Directional Light", JMESpatialType::LIGHT_DIRECTIONAL},
    {u8"🔦","Spot Light",        JMESpatialType::LIGHT_SPOT},
    {u8"🎥","Camera",            JMESpatialType::CAMERA},
    {u8"✨","Particle System",   JMESpatialType::PARTICLE},
    {u8"🔊","Audio Source",      JMESpatialType::AUDIO},
    {u8"🖼","UI Element",        JMESpatialType::UI_ELEMENT},
};

static const char* NodeIcon(const std::string& type) {
    if (type=="Geometry")         return u8"🔷";
    if (type=="Light")            return u8"💡";
    if (type=="Camera")           return u8"🎥";
    if (type=="Particle")         return u8"✨";
    if (type=="Audio")            return u8"🔊";
    return u8"📦";
}

// ─────────────────────────────────────────────────────────────
static void RenderNodeRow(SceneNode& node, int depth);
static void RenderAddPopup();

// ─────────────────────────────────────────────────────────────
void RenderHierarchyPanel() {
    auto& e     = GEditor();
    auto& state = e.panelStates["hierarchy"];
    if (!state.open) return;

    ImGuiIO& io = ImGui::GetIO();
    float panelW = state.floating ? 240.f : 230.f;
    float panelH = io.DisplaySize.y - 48.f - 36.f;   // below topbar, above statusbar

    if (!state.floating) {
        ImGui::SetNextWindowPos({0, 48.f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({panelW, panelH}, ImGuiCond_Always);
    } else {
        ImGui::SetNextWindowSize({panelW, panelH}, ImGuiCond_Once);
    }
    ImGui::SetNextWindowSizeConstraints({160, 200}, {500, 2000});

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoScrollbar;
    if (state.floating) flags &= ~ImGuiWindowFlags_NoMove;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ForgeTheme::BG1);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f, 0.f});
    ImGui::Begin("##hierarchy", nullptr, flags);

    // ── Header bar ───────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ForgeTheme::BG2);
    ImGui::BeginChild("##hier_hdr", {0, 30}, false,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos({8, 6});
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT);
    ImGui::TextUnformatted(u8"🌲");
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 5);
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT0);
    ImGui::TextUnformatted("Hierarchy");
    ImGui::PopStyleColor();

    // Scene name
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT2);
    ImGui::Text("  %s%s", e.sceneName.c_str(),
                e.sceneDirty ? " *" : "");
    ImGui::PopStyleColor();

    // Right buttons
    float rbx = ImGui::GetWindowWidth() - 70.f;
    ImGui::SameLine(rbx);
    // Float / dock toggle
    if (ForgeUI::SmallIconButton(state.floating ? u8"⊟" : u8"⊞",
                                  state.floating ? "Dock" : "Float"))
        state.floating = !state.floating;
    ImGui::SameLine(0,2);
    if (ForgeUI::SmallIconButton(u8"✕","Close Hierarchy"))
        state.open = false;

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ── Search bar ───────────────────────────────────────────
    static char searchBuf[64] = "";
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ForgeTheme::BG3);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 36);
    ImGui::SetCursorPos({4, 34});
    ImGui::InputTextWithHint("##hier_search", u8"🔍 Search...", searchBuf, 64);
    ImGui::SameLine(0,2);

    // + Add object button
    ImGui::PushStyleColor(ImGuiCol_Button,        {ForgeTheme::ACCENT.x*0.2f,
                                                    ForgeTheme::ACCENT.y*0.2f,
                                                    ForgeTheme::ACCENT.z*0.2f,1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {ForgeTheme::ACCENT.x*0.4f,
                                                    ForgeTheme::ACCENT.y*0.4f,
                                                    ForgeTheme::ACCENT.z*0.4f,1.f});
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT);
    if (ImGui::Button(u8"+##addobj", {28,24}))
        e.showNewObjPopup = true;
    ImGui::PopStyleColor(3);
    ImGui::PopStyleColor();
    ForgeUI::TooltipIfHovered("Add Game Object");

    // ── Tree view ────────────────────────────────────────────
    ImGui::SetCursorPosY(62.f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ForgeTheme::BG1);
    ImGui::BeginChild("##hier_tree", {0, 0}, false);

    std::string filter(searchBuf);
    std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

    // Build top-level nodes (parentId == -1)
    for (auto& node : e.sceneNodes) {
        if (node.parentId != -1) continue;
        if (!filter.empty()) {
            std::string lower = node.name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find(filter) == std::string::npos) continue;
        }
        RenderNodeRow(node, 0);
    }

    if (e.sceneNodes.empty()) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20);
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT2);
        float tw = ImGui::CalcTextSize("Empty scene").x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth()-tw)*0.5f);
        ImGui::TextUnformatted("Empty scene");
        ImGui::Spacing();
        float tw2 = ImGui::CalcTextSize("Press + to add objects").x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth()-tw2)*0.5f);
        ImGui::TextUnformatted("Press + to add objects");
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // Add object popup
    if (e.showNewObjPopup) RenderAddPopup();
}

// ─────────────────────────────────────────────────────────────
static void RenderNodeRow(SceneNode& node, int depth) {
    auto& e = GEditor();
    bool hasChildren = !node.childIds.empty();
    bool selected    = (e.selectedNodeId == node.id);

    ImGui::PushID(node.id);

    // Indent
    if (depth > 0) ImGui::Indent((float)depth * 14.f);

    // Row background
    ImVec2 rowMin = ImGui::GetCursorScreenPos();
    float  rowW   = ImGui::GetContentRegionAvail().x;

    // Highlight selected
    if (selected) {
        ImGui::GetWindowDrawList()->AddRectFilled(
            {rowMin.x, rowMin.y},
            {rowMin.x+rowW, rowMin.y+24},
            ImGui::ColorConvertFloat4ToU32(
                {ForgeTheme::ACCENT.x*0.18f,
                 ForgeTheme::ACCENT.y*0.18f,
                 ForgeTheme::ACCENT.z*0.18f, 1.f}),
            2.f);
        // Left accent bar
        ImGui::GetWindowDrawList()->AddRectFilled(
            {rowMin.x, rowMin.y},
            {rowMin.x+3.f, rowMin.y+24},
            ImGui::ColorConvertFloat4ToU32(ForgeTheme::ACCENT));
    }

    // Expand arrow
    float arrowX = 8.f + depth*14.f;
    ImGui::SetCursorPosX(arrowX);
    if (hasChildren) {
        ImGui::PushStyleColor(ImGuiCol_Text,
            node.expanded ? ForgeTheme::ACCENT : ForgeTheme::TEXT2);
        if (ImGui::SmallButton(node.expanded ? u8"▾" : u8"▸"))
            node.expanded = !node.expanded;
        ImGui::PopStyleColor();
    } else {
        ImGui::SetCursorPosX(arrowX + 16.f);
    }

    // Visibility eye
    ImGui::SameLine(0, 2);
    ImGui::PushStyleColor(ImGuiCol_Button,        {0,0,0,0});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0,0,0,0});
    ImGui::PushStyleColor(ImGuiCol_Text,
        node.visible ? ForgeTheme::TEXT1 : ForgeTheme::TEXT2);
    if (ImGui::SmallButton(node.visible ? u8"👁" : u8"🚫")) {
        node.visible = !node.visible;
        // TODO: GJNI().SetVisible(node.id, node.visible);
    }
    ImGui::PopStyleColor(3);

    // Icon
    ImGui::SameLine(0, 2);
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT2);
    ImGui::TextUnformatted(NodeIcon(node.type));
    ImGui::PopStyleColor();

    // Name (selectable)
    ImGui::SameLine(0, 4);
    ImGui::PushStyleColor(ImGuiCol_Text,
        selected ? ForgeTheme::TEXT0 : ForgeTheme::TEXT1);
    bool clicked = ImGui::Selectable(node.name.c_str(), selected,
                       ImGuiSelectableFlags_AllowItemOverlap, {0, 20});
    ImGui::PopStyleColor();

    if (clicked) e.selectedNodeId = node.id;

    // Context menu (long-press / right-click)
    if (ImGui::BeginPopupContextItem("##ctx")) {
        if (ImGui::MenuItem(u8"✏  Rename"))     {}
        if (ImGui::MenuItem(u8"📋  Duplicate"))  {
            GJNI().AddSpatial(JMESpatialType::BOX, node.name+"_copy", node.parentId);
        }
        if (ImGui::MenuItem(u8"📌  Focus"))      {}
        ImGui::Separator();
        if (ImGui::MenuItem(u8"📦  Add Child"))  { e.showNewObjPopup=true; }
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::DANGER);
        if (ImGui::MenuItem(u8"🗑  Delete")) {
            GJNI().RemoveSpatial(node.id);
            if (e.selectedNodeId == node.id) e.selectedNodeId = -1;
        }
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }

    if (depth > 0) ImGui::Unindent((float)depth * 14.f);

    // Recurse children
    if (hasChildren && node.expanded) {
        for (int cid : node.childIds) {
            for (auto& child : e.sceneNodes)
                if (child.id == cid) RenderNodeRow(child, depth+1);
        }
    }

    ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────
static void RenderAddPopup() {
    auto& e = GEditor();
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos({
        io.DisplaySize.x * 0.5f - 140.f,
        io.DisplaySize.y * 0.5f - 200.f
    }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({280, 400}, ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ForgeTheme::BG2);
    ImGui::Begin("##add_obj_popup", &e.showNewObjPopup,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

    // Header
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ForgeTheme::BG3);
    ImGui::BeginChild("##pop_hdr", {0,30}, false, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos({8,6});
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT);
    ImGui::TextUnformatted(u8"＋  Add Game Object");
    ImGui::PopStyleColor();
    float cx = ImGui::GetWindowWidth() - 28.f;
    ImGui::SameLine(cx);
    if (ForgeUI::SmallIconButton(u8"✕","Close")) e.showNewObjPopup=false;
    ImGui::EndChild();
    ImGui::PopStyleColor(2);

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ForgeTheme::BG1);
    ImGui::BeginChild("##obj_list", {0,0}, false);

    static char nameBuf[64] = "NewObject";
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ForgeTheme::BG3);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##objname", nameBuf, 64);
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Categories
    const char* categories[] = {
        u8"📐 Primitives", u8"💡 Lights", u8"🎬 Scene Objects"
    };
    static int selCat = 0;
    for (int i = 0; i < 3; i++) {
        bool active = (selCat == i);
        ImGui::PushStyleColor(ImGuiCol_Button,
            active ? ForgeTheme::BG4 : ForgeTheme::BG2);
        ImGui::PushStyleColor(ImGuiCol_Text,
            active ? ForgeTheme::ACCENT : ForgeTheme::TEXT1);
        if (i > 0) ImGui::SameLine(0,2);
        if (ImGui::Button(categories[i])) selCat = i;
        ImGui::PopStyleColor(2);
    }
    ImGui::Spacing();

    // Type buttons
    int rangeStart = 0, rangeEnd = 8;
    if (selCat == 1) { rangeStart=8;  rangeEnd=11; }
    if (selCat == 2) { rangeStart=11; rangeEnd=15; }

    for (int i = rangeStart; i < rangeEnd; i++) {
        const auto& ot = kObjTypes[i];
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ForgeTheme::BG3);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.f);
        ImGui::BeginChild(("##ot"+std::to_string(i)).c_str(),
                          {-1, 36}, false, ImGuiWindowFlags_NoScrollbar);
        ImGui::SetCursorPos({8,8});
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT2);
        ImGui::TextUnformatted(ot.icon);
        ImGui::PopStyleColor();
        ImGui::SameLine(0,6);
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT0);
        bool pressed = ImGui::Selectable(ot.name, false, 0, {0,20});
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        if (pressed) {
            int parentId = (e.selectedNodeId >= 0) ? -1 : -1;
            int newId    = GJNI().AddSpatial(ot.type, nameBuf, parentId);
            if (newId >= 0) {
                SceneNode n;
                n.id   = newId;
                n.name = nameBuf;
                n.type = (ot.type == JMESpatialType::NODE) ? "Node" : "Geometry";
                e.sceneNodes.push_back(n);
                e.selectedNodeId = newId;
            }
            e.showNewObjPopup = false;
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::End();
    ImGui::PopStyleColor();
}
