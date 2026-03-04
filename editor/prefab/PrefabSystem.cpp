// ============================================================
//  ForgeEngine  –  PrefabSystem.cpp
//  Prefab management: Create, Apply, Break, Edit, Library.
//  Prefabs are serialized as JSON (matching JME scene JSON).
// ============================================================

#include "../ForgeEditor.h"
#include "../../jni/JNIBridgeFull.h"
#include "../widgets/IconSystem.h"
#include "imgui.h"
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>

// ════════════════════════════════════════════════════════════
//  Prefab data
// ════════════════════════════════════════════════════════════
struct PrefabInstance {
    int         sceneNodeId;   // The root node in the scene
    int         prefabId;
    bool        modified;      // True if instance overrides prefab
};

struct PrefabDef {
    int         id;
    std::string name;
    std::string path;             // .prefab file path
    std::string thumbnailPath;
    ImVec4      color = {0.3f, 0.6f, 1.f, 1.f}; // icon tint
    // Serialized scene sub-tree (JSON)
    std::string sceneJSON;
    // List of instances in current scene
    std::vector<PrefabInstance> instances;
};

// ─── Global prefab registry ───────────────────────────────────
struct PrefabRegistry {
    std::vector<PrefabDef> prefabs;
    int nextId = 1;
    int selectedPrefab = -1;
    bool showLibrary   = false;
    bool showCreateDlg = false;
    bool showEditDlg   = false;
    char searchBuf[64] = "";
};
static PrefabRegistry g_prefabs;
static bool g_prefabInit = false;

// ════════════════════════════════════════════════════════════
//  Init with sample prefabs
// ════════════════════════════════════════════════════════════
static void InitPrefabs() {
    // Mock prefabs for demonstration
    PrefabDef p1;
    p1.id=1; p1.name="Enemy_Basic"; p1.path="/prefabs/Enemy_Basic.prefab";
    p1.color={1.f,0.3f,0.3f,1.f};
    p1.sceneJSON=R"({"id":0,"name":"Enemy_Basic","type":"Node","parent":-1})";
    g_prefabs.prefabs.push_back(p1);

    PrefabDef p2;
    p2.id=2; p2.name="Collectible_Coin"; p2.path="/prefabs/Coin.prefab";
    p2.color={1.f,0.9f,0.1f,1.f};
    g_prefabs.prefabs.push_back(p2);

    PrefabDef p3;
    p3.id=3; p3.name="Platform_1x1"; p3.path="/prefabs/Platform.prefab";
    p3.color={0.4f,0.8f,0.4f,1.f};
    g_prefabs.prefabs.push_back(p3);

    PrefabDef p4;
    p4.id=4; p4.name="SpawnPoint"; p4.path="/prefabs/SpawnPoint.prefab";
    p4.color={0.2f,0.7f,1.f,1.f};
    g_prefabs.prefabs.push_back(p4);

    g_prefabs.nextId = 5;
    g_prefabInit = true;
}

// ════════════════════════════════════════════════════════════
//  Create prefab from selected node
// ════════════════════════════════════════════════════════════
static void CreatePrefabFromNode(int nodeId, const std::string& name) {
    // Get scene JSON for this sub-tree
    std::string sceneJSON = GJNI().GetSceneTreeJSON();
    // In production: filter to only include nodeId subtree

    PrefabDef def;
    def.id   = g_prefabs.nextId++;
    def.name = name;
    def.path = GEditor().projectPath + "/prefabs/" + name + ".prefab";
    def.sceneJSON = sceneJSON;

    // Save to disk
    std::ofstream f(def.path);
    if (f.good()) f << sceneJSON;

    // Mark the scene node as a prefab instance
    for (auto& n : GEditor().sceneNodes) {
        if (n.id == nodeId) {
            n.type = "Prefab:" + name;
            break;
        }
    }

    def.instances.push_back({nodeId, def.id, false});
    g_prefabs.prefabs.push_back(def);

    GEditor().logs.push_back({
        LogLevel::INFO, "Prefab created: " + name, "now"});
}

// ════════════════════════════════════════════════════════════
//  Apply prefab to scene
// ════════════════════════════════════════════════════════════
static void InstantiatePrefab(int prefabId,
                               float px, float py, float pz) {
    PrefabDef* def = nullptr;
    for (auto& p : g_prefabs.prefabs)
        if (p.id == prefabId) { def = &p; break; }
    if (!def) return;

    int newId = GJNI().AddSpatial(JMESpatialType::NODE,
                                   def->name, -1);
    if (newId < 0) return;

    GJNI().SetTranslation(newId, px, py, pz);

    SceneNode n;
    n.id   = newId;
    n.name = def->name;
    n.type = "Prefab:"+def->name;
    n.translation[0]=px; n.translation[1]=py; n.translation[2]=pz;
    GEditor().sceneNodes.push_back(n);

    def->instances.push_back({newId, prefabId, false});
    GEditor().logs.push_back({
        LogLevel::INFO, "Instantiated: " + def->name, "now"});
}

// ════════════════════════════════════════════════════════════
//  Apply prefab changes to all instances
// ════════════════════════════════════════════════════════════
static void ApplyPrefabToAll(int prefabId) {
    PrefabDef* def = nullptr;
    for (auto& p : g_prefabs.prefabs)
        if (p.id == prefabId) { def = &p; break; }
    if (!def) return;

    // In production: re-parse sceneJSON and update each instance
    for (auto& inst : def->instances)
        inst.modified = false;

    GEditor().logs.push_back({
        LogLevel::INFO, "Applied prefab to " +
        std::to_string(def->instances.size()) + " instances", "now"});
}

// ════════════════════════════════════════════════════════════
//  Break prefab connection
// ════════════════════════════════════════════════════════════
static void BreakPrefab(int sceneNodeId) {
    for (auto& n : GEditor().sceneNodes) {
        if (n.id == sceneNodeId &&
            n.type.find("Prefab:") != std::string::npos) {
            n.type = "Node";
            // Remove from all prefab instance lists
            for (auto& p : g_prefabs.prefabs)
                p.instances.erase(
                    std::remove_if(p.instances.begin(),
                                   p.instances.end(),
                        [&](const PrefabInstance& i){
                            return i.sceneNodeId==sceneNodeId;
                        }), p.instances.end());
            GEditor().logs.push_back({
                LogLevel::WARNING,
                "Prefab connection broken for node " +
                std::to_string(sceneNodeId), "now"});
            return;
        }
    }
}

// ════════════════════════════════════════════════════════════
//  Prefab library window
// ════════════════════════════════════════════════════════════
static void DrawPrefabGrid() {
    auto& g = g_prefabs;
    std::string flt(g.searchBuf);

    float cellW = 90.f, cellH = 100.f;
    float avail = ImGui::GetContentRegionAvail().x;
    int   cols  = (int)(avail / (cellW+6));
    if (cols < 1) cols = 1;
    int col = 0;

    for (auto& p : g.prefabs) {
        if (!flt.empty() &&
            p.name.find(flt)==std::string::npos) continue;

        bool sel = (g.selectedPrefab == p.id);
        if (col>0 && col%cols!=0) ImGui::SameLine(0,6);
        col++;

        ImGui::PushID(p.id);
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
            sel ? ImVec4{p.color.x*0.25f, p.color.y*0.25f,
                         p.color.z*0.25f, 1.f}
                : ForgeTheme::BG3);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.f);
        ImGui::BeginChild("##pf", {cellW, cellH}, false,
                          ImGuiWindowFlags_NoScrollbar);

        // Icon
        ImGui::SetCursorPos({(cellW-ICON_XL)*0.5f, 8.f});
        Icons().Draw(Icon::PREFAB, ICON_XL, p.color);

        // Name
        ImGui::SetCursorPosY(ICON_XL+12);
        std::string shortName = p.name;
        if (shortName.size() > 9) shortName = shortName.substr(0,7)+"..";
        float tw = ImGui::CalcTextSize(shortName.c_str()).x;
        ImGui::SetCursorPosX((cellW-tw)*0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text,
            sel ? ForgeTheme::ACCENT : ForgeTheme::TEXT1);
        ImGui::TextUnformatted(shortName.c_str());
        ImGui::PopStyleColor();

        // Instance count badge
        if (!p.instances.empty()) {
            ImGui::SetCursorPos({2, cellH-16});
            ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT2);
            ImGui::Text("×%d", (int)p.instances.size());
            ImGui::PopStyleColor();
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        if (ImGui::IsItemClicked())
            g.selectedPrefab = p.id;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s\n%s", p.name.c_str(), p.path.c_str());

        // Drag to viewport to instantiate
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("PREFAB_ID", &p.id, sizeof(int));
            Icons().Draw(Icon::PREFAB, ICON_MD, p.color);
            ImGui::SameLine(0,4);
            ImGui::TextUnformatted(p.name.c_str());
            ImGui::EndDragDropSource();
        }

        // Context menu
        if (ImGui::BeginPopupContextItem("##pfctx")) {
            if (ImGui::MenuItem("Instantiate at Origin"))
                InstantiatePrefab(p.id, 0, 0, 0);
            if (ImGui::MenuItem("Apply to All Instances"))
                ApplyPrefabToAll(p.id);
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::DANGER);
            if (ImGui::MenuItem("Delete Prefab")) {
                g.prefabs.erase(
                    std::find_if(g.prefabs.begin(),g.prefabs.end(),
                        [&](const PrefabDef& pd){return pd.id==p.id;}));
                g.selectedPrefab = -1;
                ImGui::PopStyleColor();
                ImGui::EndPopup();
                ImGui::PopID();
                break;
            }
            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
}

// ════════════════════════════════════════════════════════════
//  MAIN RENDER
// ════════════════════════════════════════════════════════════
void RenderPrefabPanel() {
    if (!g_prefabInit) InitPrefabs();

    auto& e     = GEditor();
    auto& state = e.panelStates["prefabs"];
    if (!state.open) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize({340, 500}, ImGuiCond_Once);
    ImGui::SetNextWindowPos(
        {io.DisplaySize.x*0.5f-170, io.DisplaySize.y*0.5f-250},
        ImGuiCond_Once);

    bool open = state.open;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ForgeTheme::BG1);
    ImGui::Begin("Prefabs##prefabwin", &open);
    state.open = open;

    // ── Header ───────────────────────────────────────────────
    Icons().Draw(Icon::PREFAB, ICON_MD, ForgeTheme::ACCENT);
    ImGui::SameLine(0,5);
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT);
    ImGui::TextUnformatted("Prefab Library");
    ImGui::PopStyleColor();

    ImGui::Separator();
    ImGui::Spacing();

    // ── Toolbar ───────────────────────────────────────────────
    // Create prefab from selected node
    bool hasSelected = (e.selectedNodeId >= 0);
    ImGui::BeginDisabled(!hasSelected);
    if (Icons().Button(Icon::PREFAB, "Create prefab from selection",
                        ICON_MD, ForgeTheme::ACCENT3)) {
        g_prefabs.showCreateDlg = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine(0,4);

    if (Icons().Button(Icon::ADD, "Instantiate selected prefab",
                        ICON_MD)) {
        if (g_prefabs.selectedPrefab >= 0)
            InstantiatePrefab(g_prefabs.selectedPrefab, 0,0,0);
    }
    ImGui::SameLine(0,4);
    if (Icons().Button(Icon::PREFAB_APPLY,
                        "Apply prefab to all instances", ICON_MD)) {
        if (g_prefabs.selectedPrefab >= 0)
            ApplyPrefabToAll(g_prefabs.selectedPrefab);
    }
    ImGui::SameLine(0,4);
    if (Icons().Button(Icon::PREFAB_BREAK,
                        "Break prefab connection", ICON_MD,
                        ForgeTheme::WARNING)) {
        if (e.selectedNodeId >= 0)
            BreakPrefab(e.selectedNodeId);
    }

    ImGui::Spacing();

    // Search
    Icons().Draw(Icon::SEARCH, ICON_SM, {0.5f,0.5f,0.5f,1.f});
    ImGui::SameLine(0,4);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ForgeTheme::BG3);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##pfsrch","Filter prefabs...",
                              g_prefabs.searchBuf, 64);
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Grid view
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ForgeTheme::BG0);
    ImGui::BeginChild("##pfgrid", {0,0}, false);
    DrawPrefabGrid();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleColor();

    // ── Create dialog ─────────────────────────────────────────
    if (g_prefabs.showCreateDlg) {
        ImGui::SetNextWindowSize({300,140}, ImGuiCond_Always);
        ImGui::SetNextWindowPos(
            {io.DisplaySize.x/2-150, io.DisplaySize.y/2-70},
            ImGuiCond_Always);
        bool dlgOpen = g_prefabs.showCreateDlg;
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ForgeTheme::BG2);
        ImGui::Begin("Create Prefab##dlg", &dlgOpen,
            ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove);
        g_prefabs.showCreateDlg = dlgOpen;

        static char pfName[64] = "NewPrefab";
        ImGui::TextUnformatted("Prefab Name:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##pfn", pfName, 64);
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button,
            {ForgeTheme::ACCENT3.x*0.2f,ForgeTheme::ACCENT3.y*0.2f,
             ForgeTheme::ACCENT3.z*0.2f,1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT3);
        if (ImGui::Button("Create",{80,28})) {
            CreatePrefabFromNode(e.selectedNodeId, pfName);
            g_prefabs.showCreateDlg = false;
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine(0,4);
        if (ImGui::Button("Cancel",{80,28}))
            g_prefabs.showCreateDlg = false;

        ImGui::End();
        ImGui::PopStyleColor();
    }
}

// ─────────────────────────────────────────────────────────────
//  Inspector integration: show prefab status on selected node
// ─────────────────────────────────────────────────────────────
void DrawPrefabInspectorSection(SceneNode& node) {
    if (node.type.find("Prefab:") == std::string::npos) {
        // Option to create prefab from this node
        ImGui::Spacing();
        if (Icons().Button(Icon::PREFAB,
                "Create Prefab from this object", ICON_SM)) {
            g_prefabs.showCreateDlg = true;
        }
        return;
    }

    std::string prefabName = node.type.substr(7); // remove "Prefab:"
    PrefabDef*  def = nullptr;
    PrefabInstance* inst = nullptr;
    for (auto& p : g_prefabs.prefabs)
        if (p.name == prefabName) {
            def = &p;
            for (auto& i : p.instances)
                if (i.sceneNodeId == node.id) inst = &i;
            break;
        }

    ImGui::Spacing();
    ForgeUI::SectionHeader("Prefab");

    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT);
    Icons().Draw(Icon::PREFAB, ICON_SM, ForgeTheme::ACCENT);
    ImGui::SameLine(0,4);
    ImGui::TextUnformatted(prefabName.c_str());
    ImGui::PopStyleColor();

    if (inst && inst->modified) {
        ImGui::SameLine(0,6);
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::WARNING);
        ImGui::TextUnformatted("(modified)");
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    if (def) {
        if (Icons().Button(Icon::PREFAB_APPLY,
                "Apply overrides to prefab", ICON_SM)) {
            ApplyPrefabToAll(def->id);
        }
        ImGui::SameLine(0,4);
        if (Icons().Button(Icon::PREFAB_BREAK,
                "Break prefab connection", ICON_SM,
                ForgeTheme::WARNING)) {
            BreakPrefab(node.id);
        }
        ImGui::SameLine(0,4);
        if (Icons().Button(Icon::FOLDER_OPEN,
                "Select prefab asset", ICON_SM)) {
            g_prefabs.selectedPrefab = def->id;
            GEditor().panelStates["prefabs"].open = true;
        }
    }
}
