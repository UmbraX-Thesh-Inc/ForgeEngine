// ============================================================
//  ForgeEngine  –  BlueprintEditor.cpp
//  Unreal Engine-style visual scripting graph.
//  Features:
//    • Pinch-to-zoom (2 fingers) + pan (1 finger drag)
//    • Nodes: Event, Function, Variable, Branch, For Loop,
//             Math ops, Cast, Print, Custom Function
//    • Typed pins: Exec, Bool, Int, Float, String, Vector,
//                  Object, Wildcard
//    • Wire drawing (bezier curves) with colour per type
//    • Context menu (long press / right click) to add nodes
//    • Node selection, drag-to-move, delete
//    • Minimap overlay (bottom-right)
//    • Palette panel (left side, searchable)
// ============================================================

#include "../ForgeEditor.h"
#include "../widgets/IconSystem.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <functional>

// ════════════════════════════════════════════════════════════
//  Pin types
// ════════════════════════════════════════════════════════════
enum class PinType {
    EXEC, BOOL, INT, FLOAT, STRING, VECTOR, OBJECT, WILDCARD
};

static ImVec4 PinColor(PinType t) {
    switch(t) {
    case PinType::EXEC:     return {1.0f, 1.0f, 1.0f, 1.f};
    case PinType::BOOL:     return {0.64f,0.04f,0.04f,1.f};
    case PinType::INT:      return {0.16f,0.78f,0.16f,1.f};
    case PinType::FLOAT:    return {0.15f,0.61f,0.98f,1.f};
    case PinType::STRING:   return {0.85f,0.14f,0.69f,1.f};
    case PinType::VECTOR:   return {0.97f,0.97f,0.14f,1.f};
    case PinType::OBJECT:   return {0.08f,0.94f,0.94f,1.f};
    case PinType::WILDCARD: return {0.6f, 0.6f, 0.6f, 1.f};
    }
    return {1,1,1,1};
}
static const char* PinTypeName(PinType t) {
    switch(t) {
    case PinType::EXEC:     return "exec";
    case PinType::BOOL:     return "bool";
    case PinType::INT:      return "int";
    case PinType::FLOAT:    return "float";
    case PinType::STRING:   return "string";
    case PinType::VECTOR:   return "vector";
    case PinType::OBJECT:   return "object";
    default:                return "any";
    }
}

// ════════════════════════════════════════════════════════════
//  Pin definition
// ════════════════════════════════════════════════════════════
struct BPPin {
    int      id;
    std::string name;
    PinType  type;
    bool     isOutput;
    ImVec2   screenPos;  // set during draw
    bool     connected = false;
};

// ════════════════════════════════════════════════════════════
//  Node definition
// ════════════════════════════════════════════════════════════
enum class NodeKind {
    EVENT_BEGIN_PLAY, EVENT_TICK, EVENT_OVERLAP,
    EVENT_INPUT_KEY,  EVENT_CUSTOM,
    BRANCH, FOR_LOOP, WHILE_LOOP, SEQUENCE,
    FUNC_PRINT, FUNC_DELAY, FUNC_CUSTOM,
    MATH_ADD, MATH_SUB, MATH_MUL, MATH_DIV,
    MATH_ABS, MATH_CLAMP, MATH_LERP,
    LOGIC_AND, LOGIC_OR, LOGIC_NOT,
    VAR_GET, VAR_SET,
    CAST, GET_COMPONENT, SET_TRANSFORM,
    MAKE_VECTOR, BREAK_VECTOR,
    COMMENT
};

struct BPNode {
    int         id;
    NodeKind    kind;
    std::string title;
    std::string subtitle;
    ImVec2      pos;          // in graph space
    ImVec2      size;         // set during draw
    bool        selected = false;
    ImVec4      headerColor;
    std::vector<BPPin> inputs;
    std::vector<BPPin> outputs;
    // For comment nodes
    std::string comment;
    ImVec2      commentSize = {200, 80};
};

// ════════════════════════════════════════════════════════════
//  Wire (connection between pins)
// ════════════════════════════════════════════════════════════
struct BPWire {
    int  fromNodeId, fromPinId;
    int  toNodeId,   toPinId;
    PinType type;
};

// ════════════════════════════════════════════════════════════
//  Graph state
// ════════════════════════════════════════════════════════════
struct BlueprintGraph {
    std::vector<BPNode> nodes;
    std::vector<BPWire> wires;
    int nextId = 1;

    ImVec2 scrollOffset = {0, 0};  // pan
    float  zoom         = 1.f;
    float  targetZoom   = 1.f;

    // Dragging wire from a pin
    bool   draggingWire  = false;
    int    wireFromNode  = -1;
    int    wireFromPin   = -1;
    bool   wireFromOutput= true;
    ImVec2 wireDragEnd   = {0,0};
    PinType wireDragType = PinType::EXEC;

    // Multi-touch zoom
    float  pinchStartDist = 0.f;
    float  pinchStartZoom = 1.f;

    // Selection drag
    bool   boxSelecting  = false;
    ImVec2 boxSelStart   = {0,0};

    // Context menu
    bool   showContextMenu = false;
    ImVec2 contextPos      = {0,0};

    int NewId() { return nextId++; }
};

// ─── Global graph instance (one per blueprint asset) ─────────
static BlueprintGraph g_graph;
static bool           g_graphInit = false;

// ════════════════════════════════════════════════════════════
//  Node factory
// ════════════════════════════════════════════════════════════
static int g_pinIdCounter = 1000;
static BPPin MakePin(const char* name, PinType t, bool isOut) {
    return {g_pinIdCounter++, name, t, isOut};
}

static BPNode MakeNode(BlueprintGraph& g, NodeKind kind,
                        ImVec2 pos) {
    BPNode n;
    n.id   = g.NewId();
    n.kind = kind;
    n.pos  = pos;
    n.size = {180, 60}; // will be recalculated

    auto EP = [](const char* name){ return MakePin(name, PinType::EXEC, false); };
    auto EO = [](const char* name){ return MakePin(name, PinType::EXEC, true);  };

    switch(kind) {
    case NodeKind::EVENT_BEGIN_PLAY:
        n.title="BeginPlay"; n.subtitle="Event";
        n.headerColor={0.7f,0.1f,0.1f,1.f};
        n.outputs={EO("Exec")};
        break;
    case NodeKind::EVENT_TICK:
        n.title="Tick"; n.subtitle="Event";
        n.headerColor={0.7f,0.1f,0.1f,1.f};
        n.outputs={EO("Exec"), MakePin("Delta",PinType::FLOAT,true)};
        break;
    case NodeKind::EVENT_OVERLAP:
        n.title="OnOverlap"; n.subtitle="Event";
        n.headerColor={0.7f,0.1f,0.1f,1.f};
        n.outputs={EO("Exec"), MakePin("OtherActor",PinType::OBJECT,true)};
        break;
    case NodeKind::EVENT_INPUT_KEY:
        n.title="InputKey"; n.subtitle="Event";
        n.headerColor={0.7f,0.1f,0.1f,1.f};
        n.inputs={MakePin("Key",PinType::STRING,false)};
        n.outputs={EO("Pressed"),EO("Released")};
        break;
    case NodeKind::BRANCH:
        n.title="Branch"; n.subtitle="Flow Control";
        n.headerColor={0.2f,0.2f,0.6f,1.f};
        n.inputs={EP("Exec"), MakePin("Condition",PinType::BOOL,false)};
        n.outputs={EO("True"), EO("False")};
        break;
    case NodeKind::FOR_LOOP:
        n.title="For Loop"; n.subtitle="Flow Control";
        n.headerColor={0.2f,0.2f,0.6f,1.f};
        n.inputs={EP("Exec"), MakePin("First",PinType::INT,false),
                  MakePin("Last",PinType::INT,false)};
        n.outputs={EO("Loop Body"), MakePin("Index",PinType::INT,true),
                   EO("Completed")};
        break;
    case NodeKind::WHILE_LOOP:
        n.title="While Loop"; n.subtitle="Flow Control";
        n.headerColor={0.2f,0.2f,0.6f,1.f};
        n.inputs={EP("Exec"), MakePin("Condition",PinType::BOOL,false)};
        n.outputs={EO("Loop Body"), EO("Completed")};
        break;
    case NodeKind::SEQUENCE:
        n.title="Sequence"; n.subtitle="Flow Control";
        n.headerColor={0.2f,0.2f,0.6f,1.f};
        n.inputs={EP("Exec")};
        n.outputs={EO("Then 0"), EO("Then 1"), EO("Then 2")};
        break;
    case NodeKind::FUNC_PRINT:
        n.title="Print String"; n.subtitle="Utilities";
        n.headerColor={0.1f,0.4f,0.2f,1.f};
        n.inputs={EP("Exec"), MakePin("String",PinType::STRING,false),
                  MakePin("Duration",PinType::FLOAT,false)};
        n.outputs={EO("Exec")};
        break;
    case NodeKind::FUNC_DELAY:
        n.title="Delay"; n.subtitle="Utilities";
        n.headerColor={0.1f,0.4f,0.2f,1.f};
        n.inputs={EP("Exec"), MakePin("Duration",PinType::FLOAT,false)};
        n.outputs={EO("Completed")};
        break;
    case NodeKind::MATH_ADD:
        n.title="Add"; n.subtitle="Math";
        n.headerColor={0.15f,0.3f,0.5f,1.f};
        n.inputs={MakePin("A",PinType::WILDCARD,false),
                  MakePin("B",PinType::WILDCARD,false)};
        n.outputs={MakePin("Result",PinType::WILDCARD,true)};
        break;
    case NodeKind::MATH_SUB:
        n.title="Subtract"; n.subtitle="Math";
        n.headerColor={0.15f,0.3f,0.5f,1.f};
        n.inputs={MakePin("A",PinType::WILDCARD,false),
                  MakePin("B",PinType::WILDCARD,false)};
        n.outputs={MakePin("Result",PinType::WILDCARD,true)};
        break;
    case NodeKind::MATH_MUL:
        n.title="Multiply"; n.subtitle="Math";
        n.headerColor={0.15f,0.3f,0.5f,1.f};
        n.inputs={MakePin("A",PinType::WILDCARD,false),
                  MakePin("B",PinType::WILDCARD,false)};
        n.outputs={MakePin("Result",PinType::WILDCARD,true)};
        break;
    case NodeKind::MATH_LERP:
        n.title="Lerp"; n.subtitle="Math";
        n.headerColor={0.15f,0.3f,0.5f,1.f};
        n.inputs={MakePin("A",PinType::FLOAT,false),
                  MakePin("B",PinType::FLOAT,false),
                  MakePin("Alpha",PinType::FLOAT,false)};
        n.outputs={MakePin("Result",PinType::FLOAT,true)};
        break;
    case NodeKind::MATH_CLAMP:
        n.title="Clamp"; n.subtitle="Math";
        n.headerColor={0.15f,0.3f,0.5f,1.f};
        n.inputs={MakePin("Value",PinType::FLOAT,false),
                  MakePin("Min",PinType::FLOAT,false),
                  MakePin("Max",PinType::FLOAT,false)};
        n.outputs={MakePin("Result",PinType::FLOAT,true)};
        break;
    case NodeKind::VAR_GET:
        n.title="Get Variable"; n.subtitle="Variable";
        n.headerColor={0.4f,0.1f,0.5f,1.f};
        n.outputs={MakePin("Value",PinType::WILDCARD,true)};
        break;
    case NodeKind::VAR_SET:
        n.title="Set Variable"; n.subtitle="Variable";
        n.headerColor={0.4f,0.1f,0.5f,1.f};
        n.inputs={EP("Exec"), MakePin("Value",PinType::WILDCARD,false)};
        n.outputs={EO("Exec")};
        break;
    case NodeKind::SET_TRANSFORM:
        n.title="Set Transform"; n.subtitle="Transform";
        n.headerColor={0.2f,0.5f,0.3f,1.f};
        n.inputs={EP("Exec"), MakePin("Target",PinType::OBJECT,false),
                  MakePin("Location",PinType::VECTOR,false),
                  MakePin("Rotation",PinType::VECTOR,false),
                  MakePin("Scale",PinType::VECTOR,false)};
        n.outputs={EO("Exec")};
        break;
    case NodeKind::MAKE_VECTOR:
        n.title="Make Vector"; n.subtitle="Math";
        n.headerColor={0.15f,0.3f,0.5f,1.f};
        n.inputs={MakePin("X",PinType::FLOAT,false),
                  MakePin("Y",PinType::FLOAT,false),
                  MakePin("Z",PinType::FLOAT,false)};
        n.outputs={MakePin("Vector",PinType::VECTOR,true)};
        break;
    case NodeKind::BREAK_VECTOR:
        n.title="Break Vector"; n.subtitle="Math";
        n.headerColor={0.15f,0.3f,0.5f,1.f};
        n.inputs={MakePin("Vector",PinType::VECTOR,false)};
        n.outputs={MakePin("X",PinType::FLOAT,true),
                   MakePin("Y",PinType::FLOAT,true),
                   MakePin("Z",PinType::FLOAT,true)};
        break;
    case NodeKind::CAST:
        n.title="Cast To Object"; n.subtitle="Utilities";
        n.headerColor={0.3f,0.3f,0.1f,1.f};
        n.inputs={EP("Exec"), MakePin("Object",PinType::OBJECT,false)};
        n.outputs={EO("Success"), EO("Fail"),
                   MakePin("Result",PinType::OBJECT,true)};
        break;
    case NodeKind::COMMENT:
        n.title="Comment"; n.subtitle="";
        n.headerColor={0.25f,0.25f,0.25f,0.6f};
        n.comment="Add your comment here";
        n.size = {220, 90};
        break;
    default:
        n.title="Custom Function"; n.subtitle="Functions";
        n.headerColor={0.1f,0.4f,0.4f,1.f};
        n.inputs={EP("Exec")};
        n.outputs={EO("Exec")};
    }
    return n;
}

// ════════════════════════════════════════════════════════════
//  Init default graph
// ════════════════════════════════════════════════════════════
static void InitDefaultGraph() {
    g_graph = {};
    g_graph.nodes.push_back(MakeNode(g_graph, NodeKind::EVENT_BEGIN_PLAY, {100, 100}));
    g_graph.nodes.push_back(MakeNode(g_graph, NodeKind::FUNC_PRINT,       {360, 100}));
    g_graph.nodes.push_back(MakeNode(g_graph, NodeKind::EVENT_TICK,       {100, 280}));
    g_graph.nodes.push_back(MakeNode(g_graph, NodeKind::MATH_MUL,         {360, 280}));
    g_graph.nodes.push_back(MakeNode(g_graph, NodeKind::SET_TRANSFORM,    {600, 200}));
    // Connect BeginPlay → Print
    if (g_graph.nodes[0].outputs.size()>0 && g_graph.nodes[1].inputs.size()>0) {
        g_graph.wires.push_back({
            g_graph.nodes[0].id, g_graph.nodes[0].outputs[0].id,
            g_graph.nodes[1].id, g_graph.nodes[1].inputs[0].id,
            PinType::EXEC
        });
    }
    g_graphInit = true;
}

// ════════════════════════════════════════════════════════════
//  Coordinate helpers
// ════════════════════════════════════════════════════════════
static ImVec2 GraphToScreen(const ImVec2& gp,
                              const ImVec2& origin,
                              const ImVec2& scroll, float zoom) {
    return {origin.x + (gp.x + scroll.x) * zoom,
            origin.y + (gp.y + scroll.y) * zoom};
}
static ImVec2 ScreenToGraph(const ImVec2& sp,
                              const ImVec2& origin,
                              const ImVec2& scroll, float zoom) {
    return {(sp.x - origin.x) / zoom - scroll.x,
            (sp.y - origin.y) / zoom - scroll.y};
}

// ════════════════════════════════════════════════════════════
//  Draw a bezier wire
// ════════════════════════════════════════════════════════════
static void DrawWire(ImDrawList* dl, ImVec2 from, ImVec2 to,
                     PinType type, float alpha=1.f, float zoom=1.f) {
    ImVec4 col = PinColor(type);
    ImU32  c   = ImGui::ColorConvertFloat4ToU32(
                     {col.x, col.y, col.z, alpha});
    float  dx  = fabsf(to.x - from.x) * 0.5f + 10.f * zoom;
    float  thick = 2.5f * zoom;
    if (thick < 1.5f) thick = 1.5f;
    dl->AddBezierCubic(from, {from.x+dx, from.y},
                        {to.x-dx, to.y}, to, c,
                        thick, 24);
}

// ════════════════════════════════════════════════════════════
//  Draw pin circle
// ════════════════════════════════════════════════════════════
static void DrawPin(ImDrawList* dl, ImVec2 pos, PinType type,
                    bool connected, float zoom) {
    float r  = 6.f * zoom;
    ImU32 fc = ImGui::ColorConvertFloat4ToU32(PinColor(type));
    ImU32 bc = ImGui::ColorConvertFloat4ToU32({0.1f,0.1f,0.1f,1.f});
    if (type == PinType::EXEC) {
        // Arrow shape for exec
        float s = r * 0.85f;
        ImVec2 pts[3] = {
            {pos.x-s*0.6f, pos.y-s},
            {pos.x+s,      pos.y},
            {pos.x-s*0.6f, pos.y+s}
        };
        dl->AddTriangleFilled(pts[0],pts[1],pts[2], connected?fc:bc);
        dl->AddTriangle(pts[0],pts[1],pts[2], fc, 1.5f*zoom);
    } else {
        dl->AddCircleFilled(pos, r, connected ? fc : bc);
        dl->AddCircle(pos, r, fc, 12, 1.5f*zoom);
    }
}

// ════════════════════════════════════════════════════════════
//  Draw a single node
// ════════════════════════════════════════════════════════════
static void DrawNode(ImDrawList* dl, BPNode& node,
                     const ImVec2& origin,
                     const ImVec2& scroll, float zoom,
                     bool& anyDrag) {
    const float ROW_H  = 20.f * zoom;
    const float PAD_X  = 10.f * zoom;
    const float HDR_H  = 26.f * zoom;
    const float PIN_R  = 6.f  * zoom;

    ImVec2 sp = GraphToScreen(node.pos, origin, scroll, zoom);

    // ── Calculate node size ───────────────────────────────
    int rows  = (int)std::max(node.inputs.size(), node.outputs.size());
    float minW= 180.f * zoom;
    float nodeW = minW;
    float nodeH = HDR_H + rows * ROW_H + 12.f*zoom;
    if (node.kind == NodeKind::COMMENT)
        nodeH = node.commentSize.y * zoom;
    node.size = {nodeW/zoom, nodeH/zoom};

    ImVec2 spEnd = {sp.x+nodeW, sp.y+nodeH};

    // ── Shadow ────────────────────────────────────────────
    dl->AddRectFilled({sp.x+4,sp.y+4},{spEnd.x+4,spEnd.y+4},
        IM_COL32(0,0,0,100), 8.f*zoom);

    // ── Body ──────────────────────────────────────────────
    bool sel = node.selected;
    ImU32 bodyCol = IM_COL32(28,32,42, 240);
    ImU32 bordCol = sel
        ? ImGui::ColorConvertFloat4ToU32(ForgeTheme::ACCENT)
        : IM_COL32(60,70,90,255);
    dl->AddRectFilled(sp, spEnd, bodyCol, 7.f*zoom);

    // ── Header ────────────────────────────────────────────
    ImU32 hdrCol = ImGui::ColorConvertFloat4ToU32(
        {node.headerColor.x, node.headerColor.y,
         node.headerColor.z, node.headerColor.w});
    dl->AddRectFilled(sp, {spEnd.x, sp.y+HDR_H},
        hdrCol, 7.f*zoom);
    dl->AddRectFilled({sp.x, sp.y+HDR_H-4.f},
                      {spEnd.x, sp.y+HDR_H},
                      hdrCol, 0.f);

    // ── Header text ───────────────────────────────────────
    ImGui::SetCursorScreenPos({sp.x+PAD_X, sp.y+4.f*zoom});
    ImGui::PushStyleColor(ImGuiCol_Text, {1,1,1,1});
    ImGui::SetWindowFontScale(zoom);
    ImGui::TextUnformatted(node.title.c_str());
    if (!node.subtitle.empty()) {
        ImGui::SetCursorScreenPos({sp.x+PAD_X,
                                   sp.y+HDR_H+(-14.f*zoom)});
        ImGui::PushStyleColor(ImGuiCol_Text, {0.7f,0.7f,0.7f,1.f});
        ImGui::SetWindowFontScale(zoom * 0.75f);
        ImGui::TextUnformatted(node.subtitle.c_str());
        ImGui::SetWindowFontScale(zoom);
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.f);

    // ── Border ────────────────────────────────────────────
    dl->AddRect(sp, spEnd, bordCol, 7.f*zoom, 0, sel?2.f:1.f);

    // ── Pins ──────────────────────────────────────────────
    float rowY = sp.y + HDR_H + 8.f*zoom;

    // Input pins (left side)
    for (auto& pin : node.inputs) {
        ImVec2 pp = {sp.x, rowY + ROW_H*0.5f};
        pin.screenPos = pp;
        DrawPin(dl, pp, pin.type, pin.connected, zoom);

        if (pin.type != PinType::EXEC) {
            ImGui::SetCursorScreenPos({sp.x + PIN_R*2 + 3.f,
                                       rowY + ROW_H*0.15f});
            ImGui::PushStyleColor(ImGuiCol_Text, PinColor(pin.type));
            ImGui::SetWindowFontScale(zoom * 0.85f);
            ImGui::TextUnformatted(pin.name.c_str());
            ImGui::SetWindowFontScale(1.f);
            ImGui::PopStyleColor();
        }
        rowY += ROW_H;
    }

    // Output pins (right side)
    rowY = sp.y + HDR_H + 8.f*zoom;
    for (auto& pin : node.outputs) {
        ImVec2 pp = {spEnd.x, rowY + ROW_H*0.5f};
        pin.screenPos = pp;
        DrawPin(dl, pp, pin.type, pin.connected, zoom);

        // Label (right-aligned)
        ImVec2 ts = ImGui::CalcTextSize(pin.name.c_str());
        ts.x *= zoom * 0.85f;
        if (pin.type != PinType::EXEC) {
            ImGui::SetCursorScreenPos({
                spEnd.x - PIN_R*2 - ts.x - 3.f,
                rowY + ROW_H*0.15f});
            ImGui::PushStyleColor(ImGuiCol_Text, PinColor(pin.type));
            ImGui::SetWindowFontScale(zoom * 0.85f);
            ImGui::TextUnformatted(pin.name.c_str());
            ImGui::SetWindowFontScale(1.f);
            ImGui::PopStyleColor();
        }
        rowY += ROW_H;
    }

    // ── Drag-to-move ──────────────────────────────────────
    ImGui::SetCursorScreenPos(sp);
    ImGui::InvisibleButton(("##node"+std::to_string(node.id)).c_str(),
                            {nodeW, HDR_H});
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
        ImVec2 d = ImGui::GetIO().MouseDelta;
        node.pos.x += d.x / zoom;
        node.pos.y += d.y / zoom;
        anyDrag = true;
    }
    if (ImGui::IsItemClicked()) node.selected = true;

    // Comment node body editable
    if (node.kind == NodeKind::COMMENT) {
        ImGui::SetCursorScreenPos({sp.x+6, sp.y+HDR_H+4.f*zoom});
        ImGui::SetNextItemWidth(nodeW-12);
        char buf[256];
        strncpy(buf, node.comment.c_str(), 255);
        ImGui::SetWindowFontScale(zoom*0.85f);
        if (ImGui::InputTextMultiline(
                ("##cmt"+std::to_string(node.id)).c_str(),
                buf, 255, {nodeW-12, nodeH-HDR_H-8.f*zoom},
                ImGuiInputTextFlags_NoHorizontalScroll))
            node.comment = buf;
        ImGui::SetWindowFontScale(1.f);
    }
}

// ════════════════════════════════════════════════════════════
//  Draw minimap
// ════════════════════════════════════════════════════════════
static void DrawMinimap(ImDrawList* dl, ImVec2 vpPos, ImVec2 vpSize,
                         BlueprintGraph& g) {
    float mmW = 160.f, mmH = 100.f;
    ImVec2 mmPos = {vpPos.x+vpSize.x-mmW-12, vpPos.y+vpSize.y-mmH-12};
    dl->AddRectFilled(mmPos,{mmPos.x+mmW,mmPos.y+mmH},
        IM_COL32(12,15,22,200), 6.f);
    dl->AddRect(mmPos,{mmPos.x+mmW,mmPos.y+mmH},
        IM_COL32(60,70,90,180), 6.f);

    // Find bounds
    float mnx=1e9f,mny=1e9f,mxx=-1e9f,mxy=-1e9f;
    for (auto& n : g.nodes) {
        mnx=std::min(mnx,n.pos.x); mny=std::min(mny,n.pos.y);
        mxx=std::max(mxx,n.pos.x+n.size.x);
        mxy=std::max(mxy,n.pos.y+n.size.y);
    }
    float bw=mxx-mnx+100, bh=mxy-mny+100;
    float scx=mmW/bw, scy=mmH/bh;
    float sc=std::min(scx,scy)*0.9f;

    for (auto& n : g.nodes) {
        ImVec2 mp = {
            mmPos.x + (n.pos.x-mnx+50)*sc,
            mmPos.y + (n.pos.y-mny+50)*sc
        };
        float nw=n.size.x*sc, nh=std::max(n.size.y*sc,4.f);
        ImU32 nc=ImGui::ColorConvertFloat4ToU32(n.headerColor);
        dl->AddRectFilled(mp,{mp.x+nw,mp.y+nh},nc,2.f);
    }

    // Viewport rect on minimap
    ImVec2 vr0 = {
        mmPos.x + (-g.scrollOffset.x-mnx+50)*sc*g.zoom,
        mmPos.y + (-g.scrollOffset.y-mny+50)*sc*g.zoom
    };
    float vrw=vpSize.x/g.zoom*sc, vrh=vpSize.y/g.zoom*sc;
    dl->AddRect(vr0,{vr0.x+vrw,vr0.y+vrh},
        IM_COL32(0,210,255,180),2.f,0,1.5f);

    dl->AddText({mmPos.x+4,mmPos.y+2},
        IM_COL32(100,120,150,200),"Overview");
}

// ════════════════════════════════════════════════════════════
//  Context menu – add node
// ════════════════════════════════════════════════════════════
struct NodeCategory {
    const char* name;
    Icon        icon;
    struct Entry { const char* label; NodeKind kind; };
    std::vector<Entry> items;
};

static const std::vector<NodeCategory> kNodeCatalogue = {
    { "Events",       Icon::BP_EVENT, {
        {"BeginPlay",      NodeKind::EVENT_BEGIN_PLAY},
        {"Tick",           NodeKind::EVENT_TICK},
        {"OnOverlap",      NodeKind::EVENT_OVERLAP},
        {"InputKey",       NodeKind::EVENT_INPUT_KEY},
        {"Custom Event",   NodeKind::EVENT_CUSTOM},
    }},
    { "Flow Control", Icon::BP_FLOW, {
        {"Branch (if)",    NodeKind::BRANCH},
        {"For Loop",       NodeKind::FOR_LOOP},
        {"While Loop",     NodeKind::WHILE_LOOP},
        {"Sequence",       NodeKind::SEQUENCE},
    }},
    { "Math",         Icon::BP_MATH, {
        {"Add",            NodeKind::MATH_ADD},
        {"Subtract",       NodeKind::MATH_SUB},
        {"Multiply",       NodeKind::MATH_MUL},
        {"Clamp",          NodeKind::MATH_CLAMP},
        {"Lerp",           NodeKind::MATH_LERP},
        {"Make Vector",    NodeKind::MAKE_VECTOR},
        {"Break Vector",   NodeKind::BREAK_VECTOR},
    }},
    { "Variables",    Icon::BP_VARIABLE, {
        {"Get Variable",   NodeKind::VAR_GET},
        {"Set Variable",   NodeKind::VAR_SET},
    }},
    { "Utilities",    Icon::BP_FUNCTION, {
        {"Print String",   NodeKind::FUNC_PRINT},
        {"Delay",          NodeKind::FUNC_DELAY},
        {"Cast",           NodeKind::CAST},
        {"Set Transform",  NodeKind::SET_TRANSFORM},
    }},
    { "Comments",     Icon::BP_MACRO, {
        {"Add Comment",    NodeKind::COMMENT},
    }},
};

static void DrawContextMenu(BlueprintGraph& g) {
    if (!g.showContextMenu) return;

    ImGui::SetNextWindowPos(ImGui::GetIO().MousePos, ImGuiCond_Appearing);
    ImGui::SetNextWindowSize({260, 420}, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.08f,0.10f,0.14f,0.97f});
    ImGui::PushStyleColor(ImGuiCol_Border,   {0.f,0.831f,1.f,0.3f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  {6.f,6.f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    {4.f,3.f});

    bool open = true;
    if (ImGui::Begin("##bp_ctx", &open,
            ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove)) {

        // Search
        static char bpSearch[64] = "";
        ImGui::PushStyleColor(ImGuiCol_FrameBg, {0.15f,0.18f,0.24f,1.f});
        Icons().Draw(Icon::SEARCH, ICON_SM, {0.5f,0.5f,0.5f,1.f});
        ImGui::SameLine(0,4);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##bpsrch","Search nodes...",bpSearch,64);
        ImGui::PopStyleColor();
        ImGui::Separator();

        std::string flt(bpSearch);

        for (auto& cat : kNodeCatalogue) {
            bool anyMatch = flt.empty();
            if (!flt.empty()) {
                for (auto& it : cat.items)
                    if (std::string(it.label).find(flt)!=std::string::npos)
                        anyMatch=true;
            }
            if (!anyMatch) continue;

            ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT);
            Icons().Draw(cat.icon, ICON_SM, ForgeTheme::ACCENT);
            ImGui::SameLine(0,4);
            bool hdr = ImGui::CollapsingHeader(cat.name,
                ImGuiTreeNodeFlags_DefaultOpen);
            ImGui::PopStyleColor();

            if (hdr) {
                for (auto& it : cat.items) {
                    if (!flt.empty() &&
                        std::string(it.label).find(flt)==std::string::npos)
                        continue;
                    ImGui::SetCursorPosX(22);
                    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT0);
                    if (ImGui::Selectable(it.label, false, 0, {-1,22})) {
                        BPNode nn = MakeNode(g, it.kind, g.contextPos);
                        g.nodes.push_back(nn);
                        g.showContextMenu = false;
                        memset(bpSearch,0,sizeof(bpSearch));
                    }
                    ImGui::PopStyleColor();
                }
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    if (!open) g.showContextMenu = false;
    // Close on click outside
    if (ImGui::IsMouseClicked(0) &&
        !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
        g.showContextMenu = false;
}

// ════════════════════════════════════════════════════════════
//  Palette panel (left docked)
// ════════════════════════════════════════════════════════════
static void DrawPalette(BlueprintGraph& g, ImVec2 canvasOrigin) {
    ImGui::SetNextWindowPos({canvasOrigin.x, canvasOrigin.y});
    ImGui::SetNextWindowSize({200, 500}, ImGuiCond_Once);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.06f,0.08f,0.11f,0.95f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {6.f,6.f});
    ImGui::Begin("##bp_palette", nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove);

    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT);
    Icons().Draw(Icon::BP_FUNCTION, ICON_MD, ForgeTheme::ACCENT);
    ImGui::SameLine(0,5); ImGui::TextUnformatted("Node Palette");
    ImGui::PopStyleColor();
    ImGui::Separator();

    static char palSearch[64]="";
    ImGui::PushStyleColor(ImGuiCol_FrameBg, {0.12f,0.15f,0.20f,1.f});
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##palsrch","Filter...",palSearch,64);
    ImGui::PopStyleColor();
    ImGui::Spacing();

    std::string flt(palSearch);
    for (auto& cat : kNodeCatalogue) {
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT);
        Icons().Draw(cat.icon, ICON_SM, ForgeTheme::ACCENT);
        ImGui::SameLine(0,3);
        bool hdr = ImGui::CollapsingHeader(cat.name);
        ImGui::PopStyleColor();
        if (hdr) {
            for (auto& it : cat.items) {
                if (!flt.empty() &&
                    std::string(it.label).find(flt)==std::string::npos)
                    continue;
                ImGui::SetCursorPosX(18);
                ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT1);
                bool clicked = ImGui::Selectable(
                    it.label, false,
                    ImGuiSelectableFlags_AllowDoubleClick, {-1,22});
                ImGui::PopStyleColor();
                // Drag from palette to graph
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("BP_NODE_KIND",
                        &it.kind, sizeof(NodeKind));
                    ImGui::TextUnformatted(it.label);
                    ImGui::EndDragDropSource();
                }
                if (clicked && ImGui::IsMouseDoubleClicked(0)) {
                    // Drop at centre
                    BPNode nn = MakeNode(g, it.kind,
                        {-g.scrollOffset.x+300, -g.scrollOffset.y+200});
                    g.nodes.push_back(nn);
                }
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ════════════════════════════════════════════════════════════
//  MAIN RENDER
// ════════════════════════════════════════════════════════════
void RenderBlueprintEditor() {
    if (!g_graphInit) InitDefaultGraph();

    auto& e     = GEditor();
    auto& state = e.panelStates["blueprint"];
    if (!state.open) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize({io.DisplaySize.x, io.DisplaySize.y-48},
                             ImGuiCond_Always);
    ImGui::SetNextWindowPos({0, 48}, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.05f,0.06f,0.08f,1.f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f,0.f});

    bool open = state.open;
    ImGui::Begin("Blueprint Editor##bpwin", &open,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    state.open = open;

    ImVec2 vpPos  = ImGui::GetWindowPos();
    ImVec2 vpSize = ImGui::GetWindowSize();
    // Leave room for palette on left
    float  palW   = 200.f;
    ImVec2 cvPos  = {vpPos.x + palW, vpPos.y};
    ImVec2 cvSize = {vpSize.x - palW, vpSize.y};

    auto& g = g_graph;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ── Background grid ──────────────────────────────────────
    {
        float cellSm = 20.f * g.zoom;
        float cellLg = 100.f * g.zoom;
        ImVec2 off = {fmodf(g.scrollOffset.x*g.zoom, cellSm),
                      fmodf(g.scrollOffset.y*g.zoom, cellSm)};
        for (float x=off.x; x<cvSize.x; x+=cellSm)
            dl->AddLine({cvPos.x+x,cvPos.y},{cvPos.x+x,cvPos.y+cvSize.y},
                IM_COL32(30,35,48,180));
        for (float y=off.y; y<cvSize.y; y+=cellSm)
            dl->AddLine({cvPos.x,cvPos.y+y},{cvPos.x+cvSize.x,cvPos.y+y},
                IM_COL32(30,35,48,180));

        off = {fmodf(g.scrollOffset.x*g.zoom, cellLg),
               fmodf(g.scrollOffset.y*g.zoom, cellLg)};
        for (float x=off.x; x<cvSize.x; x+=cellLg)
            dl->AddLine({cvPos.x+x,cvPos.y},{cvPos.x+x,cvPos.y+cvSize.y},
                IM_COL32(50,60,80,160),1.5f);
        for (float y=off.y; y<cvSize.y; y+=cellLg)
            dl->AddLine({cvPos.x,cvPos.y+y},{cvPos.x+cvSize.x,cvPos.y+y},
                IM_COL32(50,60,80,160),1.5f);
    }

    // ── Clip drawing to canvas area ───────────────────────────
    dl->PushClipRect(cvPos, {cvPos.x+cvSize.x,cvPos.y+cvSize.y});

    // ── Draw wires ────────────────────────────────────────────
    for (auto& w : g.wires) {
        BPNode* fn=nullptr; BPNode* tn=nullptr;
        BPPin*  fp=nullptr; BPPin*  tp=nullptr;
        for (auto& n : g.nodes) {
            if (n.id==w.fromNodeId) {
                fn=&n;
                for (auto& p : n.outputs) if (p.id==w.fromPinId) fp=&p;
            }
            if (n.id==w.toNodeId) {
                tn=&n;
                for (auto& p : n.inputs)  if (p.id==w.toPinId)   tp=&p;
            }
        }
        if (fp && tp)
            DrawWire(dl, fp->screenPos, tp->screenPos, w.type, 0.9f, g.zoom);
    }

    // ── Dragging wire ─────────────────────────────────────────
    if (g.draggingWire)
        DrawWire(dl, g.wireDragEnd, io.MousePos,
                 g.wireDragType, 0.6f, g.zoom);

    // ── Draw nodes ────────────────────────────────────────────
    bool anyDrag = false;
    ImGui::SetCursorScreenPos(cvPos);
    for (auto& n : g.nodes)
        DrawNode(dl, n, cvPos, g.scrollOffset, g.zoom, anyDrag);

    // ── Box selection rect ────────────────────────────────────
    if (g.boxSelecting) {
        ImVec2 ms = io.MousePos;
        ImU32 bc  = IM_COL32(0,212,255,30);
        ImU32 bbc = IM_COL32(0,212,255,150);
        dl->AddRectFilled(g.boxSelStart, ms, bc);
        dl->AddRect(g.boxSelStart, ms, bbc, 0.f, 0, 1.5f);
    }

    dl->PopClipRect();

    // ── Invisible full-canvas button for pan / context ────────
    ImGui::SetCursorScreenPos(cvPos);
    ImGui::InvisibleButton("##bpcanvas", cvSize);
    bool canvasHov = ImGui::IsItemHovered();
    bool canvasAct = ImGui::IsItemActive();

    // Pan (1-finger drag on canvas)
    if (canvasAct && ImGui::IsMouseDragging(0) && !anyDrag) {
        ImVec2 d = io.MouseDelta;
        g.scrollOffset.x += d.x / g.zoom;
        g.scrollOffset.y += d.y / g.zoom;
    }

    // Pinch-to-zoom (2-finger)
    if (io.MouseDownDuration[0]>=0 && io.MouseDownDuration[1]>=0) {
        ImVec2 p0 = io.MouseClickedPos[0];
        ImVec2 p1 = io.MouseClickedPos[1];
        float  d  = hypotf(p0.x-p1.x, p0.y-p1.y);
        if (g.pinchStartDist < 1.f) {
            g.pinchStartDist = d;
            g.pinchStartZoom = g.zoom;
        } else {
            float ratio = d / g.pinchStartDist;
            g.zoom = std::clamp(g.pinchStartZoom*ratio, 0.1f, 4.f);
        }
    } else {
        g.pinchStartDist = 0.f;
    }

    // Mouse-wheel zoom (desktop / stylus)
    if (canvasHov && io.MouseWheel != 0.f) {
        float f = io.MouseWheel > 0 ? 1.1f : 0.91f;
        g.zoom = std::clamp(g.zoom*f, 0.1f, 4.f);
    }

    // Right-click → context menu
    if (canvasHov && ImGui::IsMouseClicked(1)) {
        g.contextPos = ScreenToGraph(
            io.MousePos, cvPos, g.scrollOffset, g.zoom);
        g.showContextMenu = true;
        // Deselect all
        for (auto& n : g.nodes) n.selected=false;
    }

    // Deselect on click on empty space
    if (ImGui::IsMouseClicked(0) && canvasHov && !anyDrag)
        for (auto& n : g.nodes) n.selected=false;

    // Drop from palette
    if (ImGui::BeginDragDropTarget()) {
        if (auto* pay = ImGui::AcceptDragDropPayload("BP_NODE_KIND")) {
            NodeKind k = *(NodeKind*)pay->Data;
            ImVec2 gp  = ScreenToGraph(io.MousePos, cvPos,
                                        g.scrollOffset, g.zoom);
            g.nodes.push_back(MakeNode(g, k, gp));
        }
        ImGui::EndDragDropTarget();
    }

    // Delete selected nodes (Del key)
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        g.nodes.erase(std::remove_if(g.nodes.begin(),g.nodes.end(),
            [](const BPNode& n){ return n.selected; }),
            g.nodes.end());
    }

    // ── Minimap ───────────────────────────────────────────────
    if (!g.nodes.empty())
        DrawMinimap(dl, cvPos, cvSize, g);

    // ── Toolbar overlay ───────────────────────────────────────
    ImGui::SetCursorScreenPos({cvPos.x+6, cvPos.y+6});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.06f,0.08f,0.12f,0.85f});
    ImGui::BeginChild("##bptb",{280,34},false,ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos({4,4});
    if (Icons().Button(Icon::SAVE, "Save Blueprint", ICON_MD))  {}
    ImGui::SameLine(0,2);
    if (Icons().Button(Icon::CODE_COMPILE,"Compile",ICON_MD)) {}
    ImGui::SameLine(0,2);
    if (Icons().Button(Icon::CODE_RUN,"Run",ICON_MD))  {}
    ImGui::SameLine(0,8);
    // Zoom indicator
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT2);
    ImGui::Text("%.0f%%", g.zoom*100.f);
    ImGui::PopStyleColor();
    ImGui::SameLine(0,4);
    if (ImGui::SmallButton("Reset##bpz")) {
        g.zoom=1.f; g.scrollOffset={0,0};
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ── Palette ───────────────────────────────────────────────
    DrawPalette(g, vpPos);

    // ── Context menu ─────────────────────────────────────────
    DrawContextMenu(g);

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}
