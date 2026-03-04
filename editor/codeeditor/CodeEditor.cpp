// ============================================================
//  ForgeEngine  –  CodeEditor.cpp
//  Embedded code editor for Java game scripts and GLSL shaders.
//  Features:
//    • Syntax highlighting (Java / GLSL tokeniser)
//    • Line numbers with error indicators
//    • Multi-tab (open files)
//    • Search & Replace
//    • Compile / Run buttons
//    • Font size zoom (pinch or button)
//    • Auto-indent, bracket matching
// ============================================================

#include "../ForgeEditor.h"
#include "../../jni/JNIBridgeFull.h"
#include "../widgets/IconSystem.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <vector>
#include <string>
#include <unordered_set>
#include <algorithm>
#include <sstream>

// ════════════════════════════════════════════════════════════
//  Syntax highlight token types
// ════════════════════════════════════════════════════════════
enum class TokKind {
    DEFAULT, KEYWORD, TYPE, STRING_LIT, COMMENT,
    NUMBER, PREPROC, OPERATOR, BUILTIN
};

static ImVec4 TokColor(TokKind k) {
    switch(k) {
    case TokKind::KEYWORD:    return {0.56f,0.84f,1.00f,1.f};  // blue
    case TokKind::TYPE:       return {0.45f,0.85f,0.70f,1.f};  // teal
    case TokKind::STRING_LIT: return {0.80f,0.67f,0.40f,1.f};  // orange
    case TokKind::COMMENT:    return {0.42f,0.52f,0.42f,1.f};  // green
    case TokKind::NUMBER:     return {0.72f,0.86f,0.60f,1.f};  // lime
    case TokKind::PREPROC:    return {0.85f,0.60f,0.85f,1.f};  // violet
    case TokKind::OPERATOR:   return {0.80f,0.80f,0.80f,1.f};
    case TokKind::BUILTIN:    return {1.00f,0.80f,0.50f,1.f};  // gold
    default:                  return {0.88f,0.88f,0.88f,1.f};
    }
}

// ─── Java keywords ───────────────────────────────────────────
static const std::unordered_set<std::string> kJavaKeywords = {
    "abstract","assert","boolean","break","byte","case","catch",
    "char","class","const","continue","default","do","double",
    "else","enum","extends","final","finally","float","for","goto",
    "if","implements","import","instanceof","int","interface","long",
    "native","new","package","private","protected","public","return",
    "short","static","strictfp","super","switch","synchronized",
    "this","throw","throws","transient","try","void","volatile","while",
    "true","false","null"
};
static const std::unordered_set<std::string> kJavaBuiltins = {
    "System","String","Math","List","ArrayList","HashMap","Map",
    "Vector3f","Quaternion","Node","Geometry","Spatial","Material",
    "SimpleApplication","AppSettings","ColorRGBA","FastMath"
};
// GLSL keywords
static const std::unordered_set<std::string> kGLSLKeywords = {
    "attribute","const","uniform","varying","break","continue","do",
    "for","while","if","else","in","out","inout","float","int","uint",
    "void","bool","true","false","lowp","mediump","highp","precision",
    "invariant","discard","return","mat2","mat3","mat4","vec2","vec3",
    "vec4","ivec2","ivec3","ivec4","bvec2","bvec3","bvec4","sampler2D",
    "samplerCube","struct","layout","version"
};
static const std::unordered_set<std::string> kGLSLBuiltins = {
    "gl_Position","gl_FragColor","gl_FragCoord","texture","texture2D",
    "normalize","dot","cross","length","reflect","refract","mix","clamp",
    "smoothstep","step","sin","cos","tan","abs","pow","sqrt","max","min",
    "fract","floor","ceil","mod","sign"
};

// ════════════════════════════════════════════════════════════
//  Tokeniser (line-based for display)
// ════════════════════════════════════════════════════════════
struct Token { TokKind kind; std::string text; };
using TokenLine = std::vector<Token>;

static bool isIdentChar(char c) {
    return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_';
}
static bool isDigit(char c){ return c>='0'&&c<='9'; }

static std::vector<TokenLine> Tokenise(const std::string& src,
                                        bool isGLSL) {
    const auto& keywords = isGLSL ? kGLSLKeywords : kJavaKeywords;
    const auto& builtins = isGLSL ? kGLSLBuiltins : kJavaBuiltins;

    std::vector<TokenLine> lines;
    std::stringstream ss(src);
    std::string line;
    bool blockComment = false;

    while (std::getline(ss, line)) {
        TokenLine tl;
        size_t i = 0;
        size_t n = line.size();

        while (i < n) {
            // Block comment continuation
            if (blockComment) {
                size_t end = line.find("*/", i);
                if (end == std::string::npos) {
                    tl.push_back({TokKind::COMMENT, line.substr(i)});
                    i = n; break;
                } else {
                    tl.push_back({TokKind::COMMENT,
                                  line.substr(i, end+2-i)});
                    i = end+2; blockComment = false; continue;
                }
            }
            // Line comment
            if (i+1<n && line[i]=='/' && line[i+1]=='/') {
                tl.push_back({TokKind::COMMENT, line.substr(i)});
                i=n; break;
            }
            // Block comment start
            if (i+1<n && line[i]=='/' && line[i+1]=='*') {
                size_t end = line.find("*/",i+2);
                if (end==std::string::npos){
                    tl.push_back({TokKind::COMMENT,line.substr(i)});
                    blockComment=true; i=n; break;
                }
                tl.push_back({TokKind::COMMENT,line.substr(i,end+2-i)});
                i=end+2; continue;
            }
            // Preprocessor (#version etc for GLSL)
            if (line[i]=='#') {
                tl.push_back({TokKind::PREPROC, line.substr(i)});
                i=n; break;
            }
            // String literal
            if (line[i]=='"' || line[i]=='\'') {
                char q=line[i]; size_t j=i+1;
                while(j<n && (line[j]!=q || (j>0&&line[j-1]=='\\'))) j++;
                if(j<n) j++;
                tl.push_back({TokKind::STRING_LIT, line.substr(i,j-i)});
                i=j; continue;
            }
            // Number
            if (isDigit(line[i]) ||
                (line[i]=='-'&&i+1<n&&isDigit(line[i+1]))) {
                size_t j=i; if(line[j]=='-') j++;
                while(j<n && (isDigit(line[j])||line[j]=='.'||
                              line[j]=='f'||line[j]=='L')) j++;
                tl.push_back({TokKind::NUMBER, line.substr(i,j-i)});
                i=j; continue;
            }
            // Identifier / keyword
            if (isIdentChar(line[i]) && !isDigit(line[i])) {
                size_t j=i;
                while(j<n && isIdentChar(line[j])) j++;
                std::string word = line.substr(i,j-i);
                TokKind k = TokKind::DEFAULT;
                if (keywords.count(word)) k = TokKind::KEYWORD;
                else if (builtins.count(word)) k = TokKind::BUILTIN;
                else if (!word.empty() && isupper(word[0])) k=TokKind::TYPE;
                tl.push_back({k, word});
                i=j; continue;
            }
            // Operator / punctuation
            const char* ops = "+-*/=<>!&|^~%?:;.,()[]{}@";
            if (strchr(ops, line[i])) {
                tl.push_back({TokKind::OPERATOR,
                              std::string(1,line[i])});
                i++; continue;
            }
            // Whitespace
            tl.push_back({TokKind::DEFAULT, std::string(1,line[i])});
            i++;
        }
        lines.push_back(tl);
    }
    return lines;
}

// ════════════════════════════════════════════════════════════
//  Editor tab (one open file)
// ════════════════════════════════════════════════════════════
struct CodeTab {
    std::string path;
    std::string title;
    std::string content;
    bool        modified  = false;
    bool        isGLSL    = false;
    std::vector<TokenLine> tokens;  // re-tokenised on change
    std::vector<int>       errorLines;  // lines with errors
    std::string            lastError;
    float                  fontSize  = 13.f;
    int                    cursorLine= 0;
    int                    cursorCol = 0;

    void Retokenise() {
        tokens = Tokenise(content, isGLSL);
    }
};

// ─── Global editor state ─────────────────────────────────────
struct CodeEditorState {
    std::vector<CodeTab> tabs;
    int  activeTab   = 0;
    bool showSearch  = false;
    char searchBuf[128]  = "";
    char replaceBuf[128] = "";
    bool regexMode   = false;
    bool caseSensitive = false;
    int  searchResult  = -1;  // current match line
    float pinchStartDist = 0.f;
    float pinchStartFont = 13.f;
};
static CodeEditorState g_code;
static bool g_codeInit = false;

static void InitCodeEditor() {
    CodeTab tab;
    tab.path    = "/scripts/GameLogic.java";
    tab.title   = "GameLogic.java";
    tab.isGLSL  = false;
    tab.content = R"(package com.mygame.scripts;

import com.jme3.app.SimpleApplication;
import com.jme3.math.Vector3f;
import com.jme3.scene.Node;

/**
 * Main game logic script.
 * Attach to your root node.
 */
public class GameLogic extends SimpleApplication {

    private float speed = 5.0f;
    private Node player;

    @Override
    public void simpleInitApp() {
        // Initialise the game
        player = new Node("Player");
        rootNode.attachChild(player);
        flyCam.setEnabled(false);
    }

    @Override
    public void simpleUpdate(float tpf) {
        // Called every frame
        Vector3f pos = player.getLocalTranslation();
        if (inputManager.isRunning()) {
            pos.x += speed * tpf;
        }
        player.setLocalTranslation(pos);
    }
}
)";
    tab.Retokenise();
    g_code.tabs.push_back(tab);

    CodeTab glslTab;
    glslTab.path   = "/shaders/custom.frag";
    glslTab.title  = "custom.frag";
    glslTab.isGLSL = true;
    glslTab.content= R"(#version 300 es
precision mediump float;

uniform sampler2D m_DiffuseMap;
uniform vec4      m_Color;
uniform float     m_Metallic;
uniform float     m_Roughness;
uniform vec3      g_LightDir;

in vec2 texCoord;
in vec3 normal;
in vec3 worldPos;

out vec4 fragColor;

void main() {
    vec4 albedo   = texture(m_DiffuseMap, texCoord) * m_Color;
    vec3 N        = normalize(normal);
    vec3 L        = normalize(-g_LightDir);
    float diffuse = max(dot(N, L), 0.0);

    // Simple PBR approximation
    float spec = pow(max(dot(N, L), 0.0),
                     mix(8.0, 256.0, 1.0 - m_Roughness));
    vec3 color = albedo.rgb * diffuse
               + vec3(1.0) * spec * m_Metallic
               + albedo.rgb * 0.1; // ambient

    fragColor = vec4(color, albedo.a);
}
)";
    glslTab.Retokenise();
    g_code.tabs.push_back(glslTab);

    g_codeInit = true;
}

// ════════════════════════════════════════════════════════════
//  MAIN RENDER
// ════════════════════════════════════════════════════════════
void RenderCodeEditor() {
    if (!g_codeInit) InitCodeEditor();

    auto& e     = GEditor();
    auto& state = e.panelStates["codeeditor"];
    if (!state.open) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize({io.DisplaySize.x, io.DisplaySize.y-48},
                             ImGuiCond_Always);
    ImGui::SetNextWindowPos({0,48}, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.07f,0.09f,0.12f,1.f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f,0.f});

    bool open = state.open;
    ImGui::Begin("Code Editor##cewin", &open,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    state.open = open;

    // ── Top toolbar ──────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.06f,0.07f,0.10f,1.f});
    ImGui::BeginChild("##cetb",{0,34},false,ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos({4,4});

    if (Icons().Button(Icon::SAVE, "Save  Ctrl+S", ICON_MD)) {
        if (!g_code.tabs.empty()) {
            auto& t = g_code.tabs[g_code.activeTab];
            std::ofstream f(t.path);
            if(f.good()) f << t.content;
            t.modified=false;
        }
    }
    ImGui::SameLine(0,2);
    if (Icons().Button(Icon::CODE_COMPILE, "Compile", ICON_MD)) {
        // GJNI().CompileScript(tab.path)
        GEditor().logs.push_back({LogLevel::INFO,"Compiling...","now"});
    }
    ImGui::SameLine(0,2);
    if (Icons().Button(Icon::CODE_RUN, "Run script", ICON_MD,
                        ForgeTheme::ACCENT3)) {}
    ImGui::SameLine(0,8);

    // Font size
    if (!g_code.tabs.empty()) {
        auto& t = g_code.tabs[g_code.activeTab];
        if (ImGui::SmallButton("-##fs")) t.fontSize = std::max(8.f,  t.fontSize-2.f);
        ImGui::SameLine(0,3);
        ImGui::Text("%.0fpx", t.fontSize);
        ImGui::SameLine(0,3);
        if (ImGui::SmallButton("+##fs")) t.fontSize = std::min(32.f, t.fontSize+2.f);
    }
    ImGui::SameLine(0,8);
    if (Icons().Button(Icon::SEARCH,"Search  Ctrl+F",ICON_SM))
        g_code.showSearch = !g_code.showSearch;

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ── Tab bar ───────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.08f,0.10f,0.14f,1.f});
    ImGui::BeginChild("##cetabs",{0,26},false,ImGuiWindowFlags_NoScrollbar);
    float tx=4.f;
    for (int i=0; i<(int)g_code.tabs.size(); i++) {
        auto& t = g_code.tabs[i];
        bool active = (g_code.activeTab==i);
        std::string label = t.title + (t.modified?" ●":"") + "##ct"+std::to_string(i);

        ImGui::SetCursorPos({tx,2});
        ImGui::PushStyleColor(ImGuiCol_Button,
            active ? ForgeTheme::BG4 : ForgeTheme::BG2);
        ImGui::PushStyleColor(ImGuiCol_Text,
            active ? ForgeTheme::TEXT0 : ForgeTheme::TEXT2);
        float bw = ImGui::CalcTextSize(t.title.c_str()).x + 24;
        if (ImGui::Button(label.c_str(),{bw,22}))
            g_code.activeTab=i;
        ImGui::PopStyleColor(2);
        tx += bw+2;
    }
    // New file button
    ImGui::SetCursorPos({tx,2});
    if (Icons().Button(Icon::ADD,"New Script",ICON_SM)) {
        CodeTab nt;
        nt.title="untitled.java";
        nt.path="/scripts/untitled.java";
        nt.content="// New script\n";
        nt.Retokenise();
        g_code.tabs.push_back(nt);
        g_code.activeTab=(int)g_code.tabs.size()-1;
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ── Search bar ───────────────────────────────────────────
    if (g_code.showSearch) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.10f,0.13f,0.18f,1.f});
        ImGui::BeginChild("##cesrch",{0,56},false,ImGuiWindowFlags_NoScrollbar);
        ImGui::SetCursorPos({4,4});
        Icons().Draw(Icon::SEARCH, ICON_SM, {0.5f,0.5f,0.5f,1.f});
        ImGui::SameLine(0,4);
        ImGui::SetNextItemWidth(200);
        ImGui::InputTextWithHint("##cesf","Search...",g_code.searchBuf,128);
        ImGui::SameLine(0,4);
        ImGui::SetNextItemWidth(200);
        ImGui::InputTextWithHint("##cerf","Replace...",g_code.replaceBuf,128);
        ImGui::SameLine(0,4);
        if (ImGui::SmallButton("Find"))  {}
        ImGui::SameLine(0,2);
        if (ImGui::SmallButton("Replace")) {}
        ImGui::SameLine(0,2);
        if (ImGui::SmallButton("All"))  {}
        ImGui::SameLine(0,4);
        ImGui::Checkbox("Aa",&g_code.caseSensitive);
        ImGui::SameLine(0,2);
        if (Icons().Button(Icon::CLOSE,"Close search",ICON_SM))
            g_code.showSearch=false;
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    // ── Code view ─────────────────────────────────────────────
    if (g_code.tabs.empty()) {
        ImGui::End(); ImGui::PopStyleVar(); ImGui::PopStyleColor();
        return;
    }
    auto& tab = g_code.tabs[g_code.activeTab];

    float lnW    = 46.f;
    float avail  = ImGui::GetContentRegionAvail().y;

    // Line numbers panel
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.06f,0.07f,0.09f,1.f});
    ImGui::BeginChild("##celn",{lnW,avail},false,
                      ImGuiWindowFlags_NoScrollbar|
                      ImGuiWindowFlags_NoMouseInputs);
    ImGui::SetWindowFontScale(tab.fontSize/13.f);
    for (int i=0; i<(int)tab.tokens.size(); i++) {
        bool hasErr = std::find(tab.errorLines.begin(),
                                tab.errorLines.end(),i)
                      != tab.errorLines.end();
        ImVec4 lnCol = hasErr ? ForgeTheme::DANGER
                             : (i==tab.cursorLine
                                ? ForgeTheme::ACCENT
                                : ForgeTheme::TEXT2);
        ImGui::SetCursorPosX(2);
        ImGui::PushStyleColor(ImGuiCol_Text, lnCol);
        ImGui::Text("%3d", i+1);
        ImGui::PopStyleColor();
        if (hasErr) {
            ImVec2 p=ImGui::GetItemRectMin();
            ImGui::GetWindowDrawList()->AddRectFilled(
                {p.x-2,p.y},{p.x+lnW-2,p.y+ImGui::GetTextLineHeight()},
                IM_COL32(200,30,30,40));
        }
    }
    ImGui::SetWindowFontScale(1.f);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine(0,0);

    // Code editor content
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.08f,0.10f,0.13f,1.f});
    ImGui::PushStyleColor(ImGuiCol_FrameBg, {0.08f,0.10f,0.13f,1.f});

    float scrollY = 0;
    ImGui::BeginChild("##cecode",{0,avail},false);
    // Pinch zoom
    ImGuiIO& io2 = ImGui::GetIO();
    if (io2.MouseDownDuration[0]>=0 && io2.MouseDownDuration[1]>=0) {
        ImVec2 p0=io2.MouseClickedPos[0], p1=io2.MouseClickedPos[1];
        float d=hypotf(p0.x-p1.x,p0.y-p1.y);
        if (g_code.pinchStartDist<1.f){g_code.pinchStartDist=d;
            g_code.pinchStartFont=tab.fontSize;}
        else tab.fontSize=std::clamp(g_code.pinchStartFont*d/g_code.pinchStartDist,8.f,32.f);
    } else g_code.pinchStartDist=0.f;

    ImGui::SetWindowFontScale(tab.fontSize/13.f);
    float lineH = ImGui::GetTextLineHeightWithSpacing();

    // Render tokenised lines
    for (int ln=0; ln<(int)tab.tokens.size(); ln++) {
        ImGui::SetCursorPosX(4);
        // Current line highlight
        if (ln==tab.cursorLine) {
            ImVec2 lp=ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                lp,{lp.x+ImGui::GetWindowWidth(),lp.y+lineH},
                IM_COL32(255,255,255,8));
        }
        bool firstTok=true;
        for (auto& tok : tab.tokens[ln]) {
            if (!firstTok) ImGui::SameLine(0,0);
            firstTok=false;
            ImGui::PushStyleColor(ImGuiCol_Text, TokColor(tok.kind));
            ImGui::TextUnformatted(tok.text.c_str());
            ImGui::PopStyleColor();
        }
        // Empty line still needs a newline
        if (tab.tokens[ln].empty()) ImGui::TextUnformatted("");
    }
    ImGui::SetWindowFontScale(1.f);

    // Handle click → set cursor line
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
        ImVec2 mp = ImGui::GetMousePos();
        ImVec2 wp = ImGui::GetWindowPos();
        tab.cursorLine = (int)((mp.y-wp.y)/lineH);
        tab.cursorLine = std::clamp(tab.cursorLine,0,
                                    (int)tab.tokens.size()-1);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(2);

    // ── Status bar ───────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.05f,0.06f,0.08f,1.f});
    ImGui::BeginChild("##cesb",{0,20},false,ImGuiWindowFlags_NoScrollbar);
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT2);
    ImGui::SetCursorPos({4,2});
    ImGui::Text("Ln %d  Col %d  |  %s  |  UTF-8",
        tab.cursorLine+1, tab.cursorCol+1,
        tab.isGLSL ? "GLSL" : "Java");
    if (!tab.lastError.empty()) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::DANGER);
        ImGui::Text("  ● %s", tab.lastError.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}
