// ============================================================
//  ForgeEngine  –  MaterialEditor.cpp
//  Visual PBR material editor.
//  • Node graph (Texture, Constant, Math, Output)
//  • Live preview sphere with PBR shading
//  • Property panel: albedo, metallic, roughness, etc.
//  • Pinch-to-zoom, pan
//  • Export to JME j3m / j3md
// ============================================================

#include "../ForgeEditor.h"
#include "../widgets/IconSystem.h"
#include "imgui.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

// ════════════════════════════════════════════════════════════
//  Material node types
// ════════════════════════════════════════════════════════════
enum class MatNodeKind {
    OUTPUT,       // Final PBR output (Albedo, Metallic, Roughness, Normal, AO, Emissive)
    TEXTURE,      // Texture sampler
    CONSTANT_F,   // Float constant
    CONSTANT_V4,  // Vec4 / colour
    MULTIPLY,     // A * B
    ADD,          // A + B
    LERP,         // mix(A,B,t)
    FRESNEL,      // Fresnel effect
    NORMAL_MAP,   // Normal map decode
    AO_ROUGHNESS, // Combined AO+Roughness texture
    TIME,         // Time node (for animations)
    UV_SCALE,     // UV transform
};

static const char* MatNodeName(MatNodeKind k) {
    switch(k) {
    case MatNodeKind::OUTPUT:       return "Material Output";
    case MatNodeKind::TEXTURE:      return "Texture Sample";
    case MatNodeKind::CONSTANT_F:   return "Scalar";
    case MatNodeKind::CONSTANT_V4:  return "Color";
    case MatNodeKind::MULTIPLY:     return "Multiply";
    case MatNodeKind::ADD:          return "Add";
    case MatNodeKind::LERP:         return "Lerp";
    case MatNodeKind::FRESNEL:      return "Fresnel";
    case MatNodeKind::NORMAL_MAP:   return "Normal Map";
    case MatNodeKind::AO_ROUGHNESS: return "AO+Roughness";
    case MatNodeKind::TIME:         return "Time";
    case MatNodeKind::UV_SCALE:     return "UV Scale";
    }
    return "Node";
}

// ════════════════════════════════════════════════════════════
//  Material pin
// ════════════════════════════════════════════════════════════
enum class MatSlot {
    // Output node inputs
    ALBEDO, METALLIC, ROUGHNESS, NORMAL, AO, EMISSIVE, OPACITY,
    // Math
    A, B, T, VALUE,
    // Generic
    RGB, R, G, B_, A_, UV,
    // Output
    RESULT, COLOR, FLOAT_, VECTOR
};

static ImVec4 MatSlotColor(MatSlot s) {
    switch(s) {
    case MatSlot::ALBEDO:    return {0.95f,0.70f,0.20f,1.f};
    case MatSlot::METALLIC:  return {0.60f,0.60f,0.60f,1.f};
    case MatSlot::ROUGHNESS: return {0.85f,0.45f,0.15f,1.f};
    case MatSlot::NORMAL:    return {0.30f,0.50f,1.00f,1.f};
    case MatSlot::EMISSIVE:  return {1.00f,0.40f,0.05f,1.f};
    case MatSlot::RGB:       return {0.80f,0.50f,0.80f,1.f};
    case MatSlot::FLOAT_:    return {0.20f,0.75f,0.20f,1.f};
    default:                 return {0.60f,0.60f,0.60f,1.f};
    }
}
static const char* MatSlotName(MatSlot s) {
    switch(s) {
    case MatSlot::ALBEDO:    return "Albedo";
    case MatSlot::METALLIC:  return "Metallic";
    case MatSlot::ROUGHNESS: return "Roughness";
    case MatSlot::NORMAL:    return "Normal";
    case MatSlot::AO:        return "AO";
    case MatSlot::EMISSIVE:  return "Emissive";
    case MatSlot::OPACITY:   return "Opacity";
    case MatSlot::A:         return "A";
    case MatSlot::B:         return "B";
    case MatSlot::T:         return "Alpha";
    case MatSlot::RGB:       return "RGB";
    case MatSlot::RESULT:    return "Result";
    case MatSlot::COLOR:     return "Color";
    case MatSlot::FLOAT_:    return "Value";
    case MatSlot::UV:        return "UV";
    default:                 return "Pin";
    }
}

struct MatPin {
    int     id;
    MatSlot slot;
    bool    isOutput;
    ImVec2  screenPos;
    bool    connected = false;
};

// ════════════════════════════════════════════════════════════
//  Material node
// ════════════════════════════════════════════════════════════
struct MatNode {
    int         id;
    MatNodeKind kind;
    ImVec2      pos;
    ImVec2      size;
    bool        selected = false;
    // Editable values
    float       floatVal  = 0.5f;
    float       colorVal[4] = {1,1,1,1};
    std::string texturePath;
    float       uvScale[2] = {1,1};
    std::vector<MatPin> inputs;
    std::vector<MatPin> outputs;
};

struct MatWire {
    int fromNode, fromPin;
    int toNode,   toPin;
    MatSlot slotType;
};

// ════════════════════════════════════════════════════════════
//  Material graph
// ════════════════════════════════════════════════════════════
struct MaterialGraph {
    std::vector<MatNode> nodes;
    std::vector<MatWire> wires;
    int nextId = 1;
    ImVec2 scroll = {0,0};
    float  zoom   = 1.f;
    float  pinchStartDist = 0.f;
    float  pinchStartZoom = 1.f;
    int    outputNodeId   = 1;
    std::string materialName = "M_NewMaterial";
    bool   dirty = false;

    // PBR preview params (driven by constant nodes when disconnected)
    float previewAlbedo[4] = {0.8f,0.4f,0.1f,1.f};
    float previewMetallic  = 0.0f;
    float previewRoughness = 0.5f;
};

static MaterialGraph g_matGraph;
static bool          g_matInit = false;
static int           g_matPinId = 5000;

static MatPin MMP(MatSlot s, bool out) {
    return {g_matPinId++, s, out};
}

static MatNode MakeMaterialNode(MaterialGraph& g,
                                 MatNodeKind kind,
                                 ImVec2 pos) {
    MatNode n; n.id=g.nextId++; n.kind=kind; n.pos=pos;
    switch(kind) {
    case MatNodeKind::OUTPUT:
        n.inputs = {MMP(MatSlot::ALBEDO,false), MMP(MatSlot::METALLIC,false),
                    MMP(MatSlot::ROUGHNESS,false), MMP(MatSlot::NORMAL,false),
                    MMP(MatSlot::AO,false), MMP(MatSlot::EMISSIVE,false),
                    MMP(MatSlot::OPACITY,false)};
        break;
    case MatNodeKind::TEXTURE:
        n.inputs  = {MMP(MatSlot::UV,false)};
        n.outputs = {MMP(MatSlot::RGB,true), MMP(MatSlot::FLOAT_,true)};
        n.texturePath = "(none)";
        break;
    case MatNodeKind::CONSTANT_F:
        n.outputs = {MMP(MatSlot::FLOAT_,true)};
        n.floatVal = 0.5f;
        break;
    case MatNodeKind::CONSTANT_V4:
        n.outputs = {MMP(MatSlot::COLOR,true)};
        break;
    case MatNodeKind::MULTIPLY:
    case MatNodeKind::ADD:
        n.inputs  = {MMP(MatSlot::A,false), MMP(MatSlot::B,false)};
        n.outputs = {MMP(MatSlot::RESULT,true)};
        break;
    case MatNodeKind::LERP:
        n.inputs  = {MMP(MatSlot::A,false), MMP(MatSlot::B,false),
                     MMP(MatSlot::T,false)};
        n.outputs = {MMP(MatSlot::RESULT,true)};
        break;
    case MatNodeKind::FRESNEL:
        n.outputs = {MMP(MatSlot::FLOAT_,true)};
        n.floatVal = 5.f;
        break;
    case MatNodeKind::NORMAL_MAP:
        n.inputs  = {MMP(MatSlot::RGB,false)};
        n.outputs = {MMP(MatSlot::NORMAL,true)};
        n.floatVal = 1.f;
        break;
    case MatNodeKind::UV_SCALE:
        n.outputs = {MMP(MatSlot::UV,true)};
        n.uvScale[0]=1; n.uvScale[1]=1;
        break;
    case MatNodeKind::TIME:
        n.outputs = {MMP(MatSlot::FLOAT_,true)};
        break;
    default: break;
    }
    return n;
}

static void InitDefaultMaterialGraph() {
    g_matGraph = {};
    // Output node (fixed)
    MatNode out = MakeMaterialNode(g_matGraph, MatNodeKind::OUTPUT, {500,200});
    g_matGraph.outputNodeId = out.id;
    g_matGraph.nodes.push_back(out);
    // Default albedo colour
    MatNode alb = MakeMaterialNode(g_matGraph, MatNodeKind::CONSTANT_V4, {200,100});
    alb.colorVal[0]=0.8f; alb.colorVal[1]=0.4f; alb.colorVal[2]=0.1f;
    g_matGraph.nodes.push_back(alb);
    // Metallic
    MatNode met = MakeMaterialNode(g_matGraph, MatNodeKind::CONSTANT_F, {200,220});
    met.floatVal=0.0f;
    g_matGraph.nodes.push_back(met);
    // Roughness
    MatNode rou = MakeMaterialNode(g_matGraph, MatNodeKind::CONSTANT_F, {200,290});
    rou.floatVal=0.5f;
    g_matGraph.nodes.push_back(rou);
    g_matInit = true;
}

// ════════════════════════════════════════════════════════════
//  Draw material node
// ════════════════════════════════════════════════════════════
static void DrawMatNode(ImDrawList* dl, MatNode& node,
                         ImVec2 origin, ImVec2 scroll, float zoom) {
    const float ROW_H = 22.f * zoom;
    const float HDR_H = 26.f * zoom;
    const float PAD   = 10.f * zoom;

    ImVec2 sp = {origin.x + (node.pos.x+scroll.x)*zoom,
                 origin.y + (node.pos.y+scroll.y)*zoom};

    int rows = (int)std::max(node.inputs.size(), node.outputs.size());
    float nW = 200.f * zoom;
    float nH = HDR_H + rows*ROW_H + 16.f*zoom;

    // Extra height for inline editors
    if (node.kind==MatNodeKind::TEXTURE)     nH+=40.f*zoom;
    if (node.kind==MatNodeKind::CONSTANT_V4) nH+=30.f*zoom;
    if (node.kind==MatNodeKind::CONSTANT_F)  nH+=20.f*zoom;
    if (node.kind==MatNodeKind::NORMAL_MAP)  nH+=20.f*zoom;

    node.size = {nW/zoom, nH/zoom};
    ImVec2 spEnd = {sp.x+nW, sp.y+nH};

    bool isOutput = (node.kind==MatNodeKind::OUTPUT);
    ImVec4 hdrCol = isOutput
        ? ImVec4{0.1f,0.5f,0.2f,1.f}
        : ImVec4{0.12f,0.22f,0.38f,1.f};

    // Shadow
    dl->AddRectFilled({sp.x+3,sp.y+3},{spEnd.x+3,spEnd.y+3},
        IM_COL32(0,0,0,90), 7.f*zoom);
    // Body
    dl->AddRectFilled(sp, spEnd, IM_COL32(24,28,38,245), 7.f*zoom);
    // Header
    dl->AddRectFilled(sp,{spEnd.x,sp.y+HDR_H},
        ImGui::ColorConvertFloat4ToU32(hdrCol), 7.f*zoom);
    dl->AddRectFilled({sp.x,sp.y+HDR_H-4},{spEnd.x,sp.y+HDR_H},
        ImGui::ColorConvertFloat4ToU32(hdrCol));
    // Border
    ImU32 bord = node.selected
        ? ImGui::ColorConvertFloat4ToU32(ForgeTheme::ACCENT)
        : IM_COL32(55,65,85,255);
    dl->AddRect(sp,spEnd,bord,7.f*zoom,0,node.selected?2.f:1.f);

    // Title
    ImGui::SetCursorScreenPos({sp.x+6, sp.y+5*zoom});
    ImGui::SetWindowFontScale(zoom*0.9f);
    ImGui::PushStyleColor(ImGuiCol_Text, {1,1,1,1});
    ImGui::TextUnformatted(MatNodeName(node.kind));
    ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.f);

    float curY = sp.y + HDR_H + 6.f*zoom;

    // ── Inline editors ────────────────────────────────────────
    if (node.kind==MatNodeKind::CONSTANT_V4) {
        ImGui::SetCursorScreenPos({sp.x+8, curY});
        ImGui::SetNextItemWidth(nW-16);
        char cbuf[32]; snprintf(cbuf,32,"##clr%d",node.id);
        ImGui::ColorEdit4(cbuf, node.colorVal,
            ImGuiColorEditFlags_NoLabel|ImGuiColorEditFlags_NoSidePreview);
        curY += 30.f*zoom;
    }
    if (node.kind==MatNodeKind::CONSTANT_F ||
        node.kind==MatNodeKind::FRESNEL    ||
        node.kind==MatNodeKind::NORMAL_MAP) {
        ImGui::SetCursorScreenPos({sp.x+8, curY});
        ImGui::SetNextItemWidth(nW-16);
        char fb[32]; snprintf(fb,32,"##fv%d",node.id);
        ImGui::SliderFloat(fb,&node.floatVal,0.f,1.f);
        curY += 22.f*zoom;
    }
    if (node.kind==MatNodeKind::TEXTURE) {
        ImGui::SetCursorScreenPos({sp.x+8, curY});
        ImGui::SetNextItemWidth(nW-16);
        char tb[64]; strncpy(tb,node.texturePath.c_str(),63);
        char tid[32]; snprintf(tid,32,"##tex%d",node.id);
        if (ImGui::InputText(tid,tb,64)) node.texturePath=tb;
        curY += 42.f*zoom;
    }
    if (node.kind==MatNodeKind::UV_SCALE) {
        ImGui::SetCursorScreenPos({sp.x+8, curY});
        ImGui::SetNextItemWidth(nW/2-10);
        char u[32]; snprintf(u,32,"##us%d",node.id);
        ImGui::DragFloat(u,&node.uvScale[0],0.1f,0.01f,100.f,"U:%.2f");
        ImGui::SameLine(0,4);
        ImGui::SetNextItemWidth(nW/2-10);
        char v[32]; snprintf(v,32,"##vs%d",node.id);
        ImGui::DragFloat(v,&node.uvScale[1],0.1f,0.01f,100.f,"V:%.2f");
        curY += 22.f*zoom;
    }

    // ── Pins ──────────────────────────────────────────────────
    float pinY = curY;
    float pinR = 6.f*zoom;

    // Input pins (left)
    for (auto& pin : node.inputs) {
        ImVec4 pc = MatSlotColor(pin.slot);
        ImVec2 pp = {sp.x, pinY+ROW_H*0.5f};
        pin.screenPos = pp;
        ImU32 fc2 = ImGui::ColorConvertFloat4ToU32(pc);
        if (pin.connected) dl->AddCircleFilled(pp,pinR,fc2);
        else               dl->AddCircle(pp,pinR,fc2,12,1.5f);

        ImGui::SetCursorScreenPos({sp.x+pinR*2+3, pinY+ROW_H*0.15f});
        ImGui::SetWindowFontScale(zoom*0.8f);
        ImGui::PushStyleColor(ImGuiCol_Text, pc);
        ImGui::TextUnformatted(MatSlotName(pin.slot));
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.f);
        pinY += ROW_H;
    }

    // Output pins (right)
    pinY = curY;
    for (auto& pin : node.outputs) {
        ImVec4 pc = MatSlotColor(pin.slot);
        ImVec2 pp = {spEnd.x, pinY+ROW_H*0.5f};
        pin.screenPos = pp;
        ImU32 fc2 = ImGui::ColorConvertFloat4ToU32(pc);
        if (pin.connected) dl->AddCircleFilled(pp,pinR,fc2);
        else               dl->AddCircle(pp,pinR,fc2,12,1.5f);

        std::string lbl = MatSlotName(pin.slot);
        ImVec2 ts = ImGui::CalcTextSize(lbl.c_str());
        ImGui::SetCursorScreenPos({spEnd.x-pinR*2-ts.x-3, pinY+ROW_H*0.15f});
        ImGui::SetWindowFontScale(zoom*0.8f);
        ImGui::PushStyleColor(ImGuiCol_Text, pc);
        ImGui::TextUnformatted(lbl.c_str());
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.f);
        pinY += ROW_H;
    }

    // Drag header to move
    ImGui::SetCursorScreenPos(sp);
    ImGui::InvisibleButton(("##mn"+std::to_string(node.id)).c_str(),
                            {nW, HDR_H});
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
        ImVec2 d = ImGui::GetIO().MouseDelta;
        if (node.kind != MatNodeKind::OUTPUT) {
            node.pos.x += d.x/zoom;
            node.pos.y += d.y/zoom;
        }
    }
    if (ImGui::IsItemClicked()) node.selected=true;
}

// ════════════════════════════════════════════════════════════
//  Preview sphere (simplified – draws a circle with PBR tint)
// ════════════════════════════════════════════════════════════
static void DrawPreviewSphere(ImDrawList* dl, ImVec2 pos,
                               float r, MaterialGraph& g) {
    // Approximate PBR: lerp albedo by metallic, darken by roughness
    float* alb = g.previewAlbedo;
    float  met = g.previewMetallic;
    float  rou = g.previewRoughness;

    ImVec2 lightDir = {0.7f, -0.7f}; // normalised
    // Draw gradient circle simulating a lit sphere
    int segs = 64;
    for (int i=0; i<segs; i++) {
        float a0 = (float)i     / segs * 6.2832f;
        float a1 = (float)(i+1) / segs * 6.2832f;
        // Normal at centre of segment
        float nx = cosf((a0+a1)*0.5f);
        float ny = sinf((a0+a1)*0.5f);
        float diff = std::max(0.f, nx*lightDir.x + ny*lightDir.y + 0.5f);
        float spec = powf(diff, 1.f/(rou+0.01f)) * met;
        float lum  = diff * (1.f-met*0.8f) + spec;
        ImVec4 c = {
            std::min(1.f, alb[0]*lum + spec),
            std::min(1.f, alb[1]*lum + spec),
            std::min(1.f, alb[2]*lum + spec), 1.f
        };
        dl->PathArcTo(pos, r, a0, a1, 1);
        dl->PathLineTo(pos);
        dl->PathFillConvex(ImGui::ColorConvertFloat4ToU32(c));
    }
    // Specular highlight
    dl->AddCircle(pos, r, IM_COL32(80,100,130,100), 64, 1.5f);
    // Rim light
    for (int i=0; i<segs; i++) {
        float a0=(float)i/segs*6.2832f, a1=(float)(i+1)/segs*6.2832f;
        float rim=powf(fabsf(sinf((a0+a1)*0.5f)),3.f)*0.5f;
        ImU32 rc=IM_COL32((int)(rim*80),(int)(rim*150),(int)(rim*200),180);
        dl->PathArcTo(pos,r,a0,a1,1);
        dl->PathStroke(rc,false,1.5f);
    }
}

// ════════════════════════════════════════════════════════════
//  MAIN RENDER
// ════════════════════════════════════════════════════════════
void RenderMaterialEditor() {
    if (!g_matInit) InitDefaultMaterialGraph();

    auto& e     = GEditor();
    auto& state = e.panelStates["material"];
    if (!state.open) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize({io.DisplaySize.x, io.DisplaySize.y-48},
                             ImGuiCond_Always);
    ImGui::SetNextWindowPos({0,48}, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.04f,0.05f,0.07f,1.f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f,0.f});

    bool open = state.open;
    ImGui::Begin("Material Editor##matwin", &open,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    state.open = open;

    ImVec2 vpPos  = ImGui::GetWindowPos();
    ImVec2 vpSize = ImGui::GetWindowSize();
    auto&  g      = g_matGraph;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Layout: left=preview+props(240), right=graph
    float  leftW  = 240.f;
    ImVec2 cvPos  = {vpPos.x+leftW, vpPos.y};
    ImVec2 cvSize = {vpSize.x-leftW, vpSize.y};

    // ── Left panel: preview + properties ─────────────────────
    ImGui::SetCursorScreenPos(vpPos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.07f,0.09f,0.13f,1.f});
    ImGui::BeginChild("##mat_left",{leftW,-1},false);

    // Material name
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT);
    Icons().Draw(Icon::ASSET_MATERIAL, ICON_MD, ForgeTheme::ACCENT);
    ImGui::SameLine(0,5);
    ImGui::TextUnformatted("Material Editor");
    ImGui::PopStyleColor();
    ImGui::Separator();

    char nameBuf[64]; strncpy(nameBuf,g.materialName.c_str(),63);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##matname",nameBuf,64)) g.materialName=nameBuf;
    ImGui::Spacing();

    // Preview sphere
    float  sphereR = 80.f;
    ImVec2 sphereC = {vpPos.x + leftW/2,
                      ImGui::GetCursorScreenPos().y + sphereR + 10.f};
    dl->AddRectFilled(
        {vpPos.x, sphereC.y-sphereR-10},
        {vpPos.x+leftW, sphereC.y+sphereR+10},
        IM_COL32(8,10,14,255));
    DrawPreviewSphere(dl, sphereC, sphereR, g);
    ImGui::Dummy({leftW, sphereR*2+20});

    ImGui::Separator();
    ForgeUI::SectionHeader("PBR Parameters");

    // Albedo
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT1);
    ImGui::TextUnformatted("Albedo"); ImGui::PopStyleColor();
    ImGui::SameLine(70); ImGui::SetNextItemWidth(-1);
    ImGui::ColorEdit4("##alb",g.previewAlbedo,ImGuiColorEditFlags_NoLabel);

    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT1);
    ImGui::TextUnformatted("Metallic"); ImGui::PopStyleColor();
    ImGui::SameLine(70); ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##met",&g.previewMetallic,0.f,1.f);

    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT1);
    ImGui::TextUnformatted("Roughness"); ImGui::PopStyleColor();
    ImGui::SameLine(70); ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##rou",&g.previewRoughness,0.f,1.f);

    ImGui::Spacing();
    ForgeUI::SectionHeader("Add Node");
    const struct { MatNodeKind k; Icon ic; const char* lbl; } addNodes[]={
        {MatNodeKind::TEXTURE,     Icon::MAT_TEXTURE,  "Texture"},
        {MatNodeKind::CONSTANT_V4, Icon::MAT_CONSTANT, "Color"},
        {MatNodeKind::CONSTANT_F,  Icon::MAT_CONSTANT, "Scalar"},
        {MatNodeKind::MULTIPLY,    Icon::MAT_MULTIPLY, "Multiply"},
        {MatNodeKind::ADD,         Icon::MAT_ADD,      "Add"},
        {MatNodeKind::LERP,        Icon::MAT_MULTIPLY, "Lerp"},
        {MatNodeKind::FRESNEL,     Icon::MAT_CONSTANT, "Fresnel"},
        {MatNodeKind::NORMAL_MAP,  Icon::MAT_TEXTURE,  "Normal Map"},
        {MatNodeKind::UV_SCALE,    Icon::MAT_CONSTANT, "UV Scale"},
        {MatNodeKind::TIME,        Icon::MAT_CONSTANT, "Time"},
    };
    for (auto& an : addNodes) {
        Icons().Draw(an.ic, ICON_SM, ForgeTheme::ACCENT);
        ImGui::SameLine(0,4);
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT0);
        if (ImGui::Selectable(an.lbl, false, 0, {-1,20})) {
            ImVec2 gp = {-g.scroll.x+300.f+(float)(rand()%100),
                         -g.scroll.y+150.f+(float)(rand()%200)};
            g.nodes.push_back(MakeMaterialNode(g, an.k, gp));
        }
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button,
        {ForgeTheme::ACCENT3.x*0.2f,ForgeTheme::ACCENT3.y*0.2f,
         ForgeTheme::ACCENT3.z*0.2f,1.f});
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT3);
    if (ImGui::Button("Save Material##mat",{-1,30})) { /* export .j3m */ }
    ImGui::PopStyleColor(2);

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ── Graph canvas ──────────────────────────────────────────
    // Background
    for (float x=fmodf(g.scroll.x*g.zoom,20.f); x<cvSize.x; x+=20.f*g.zoom)
        dl->AddLine({cvPos.x+x,cvPos.y},{cvPos.x+x,cvPos.y+cvSize.y},
            IM_COL32(28,33,46,150));
    for (float y=fmodf(g.scroll.y*g.zoom,20.f); y<cvSize.y; y+=20.f*g.zoom)
        dl->AddLine({cvPos.x,cvPos.y+y},{cvPos.x+cvSize.x,cvPos.y+y},
            IM_COL32(28,33,46,150));

    dl->PushClipRect(cvPos,{cvPos.x+cvSize.x,cvPos.y+cvSize.y});

    // Draw wires
    for (auto& w : g.wires) {
        MatNode* fn=nullptr; MatNode* tn=nullptr;
        MatPin*  fp=nullptr; MatPin*  tp=nullptr;
        for (auto& n : g.nodes) {
            if (n.id==w.fromNode) { fn=&n; for (auto& p:n.outputs) if(p.id==w.fromPin) fp=&p; }
            if (n.id==w.toNode)   { tn=&n; for (auto& p:n.inputs)  if(p.id==w.toPin)   tp=&p; }
        }
        if (fp && tp) {
            ImVec4 wc = MatSlotColor(w.slotType);
            ImU32 c2 = ImGui::ColorConvertFloat4ToU32(wc);
            float dx = fabsf(tp->screenPos.x-fp->screenPos.x)*0.5f+10.f;
            dl->AddBezierCubic(fp->screenPos,
                {fp->screenPos.x+dx,fp->screenPos.y},
                {tp->screenPos.x-dx,tp->screenPos.y},
                tp->screenPos, c2, 2.f*g.zoom, 24);
        }
    }

    // Draw nodes
    for (auto& n : g.nodes)
        DrawMatNode(dl, n, cvPos, g.scroll, g.zoom);

    dl->PopClipRect();

    // Pan
    ImGui::SetCursorScreenPos(cvPos);
    ImGui::InvisibleButton("##matcanvas",cvSize);
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
        ImVec2 d = io.MouseDelta;
        g.scroll.x += d.x/g.zoom;
        g.scroll.y += d.y/g.zoom;
    }
    // Wheel zoom
    if (ImGui::IsItemHovered() && io.MouseWheel!=0.f) {
        float f = io.MouseWheel>0 ? 1.1f : 0.91f;
        g.zoom = std::clamp(g.zoom*f, 0.15f, 4.f);
    }
    // Pinch zoom
    if (io.MouseDownDuration[0]>=0 && io.MouseDownDuration[1]>=0) {
        ImVec2 p0=io.MouseClickedPos[0], p1=io.MouseClickedPos[1];
        float  d2=hypotf(p0.x-p1.x,p0.y-p1.y);
        if (g.pinchStartDist<1.f){g.pinchStartDist=d2; g.pinchStartZoom=g.zoom;}
        else g.zoom=std::clamp(g.pinchStartZoom*d2/g.pinchStartDist,0.15f,4.f);
    } else g.pinchStartDist=0.f;

    // Zoom label
    dl->AddText({cvPos.x+cvSize.x-60, cvPos.y+8},
        IM_COL32(80,100,130,200),
        (std::to_string((int)(g.zoom*100))+"%").c_str());

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}
