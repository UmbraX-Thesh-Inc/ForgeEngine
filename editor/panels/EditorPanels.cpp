// ============================================================
//  ForgeEngine  –  ConsolePanel.cpp
//  Log console with level filter, search, clear, copy
// ============================================================

#include "../ForgeEditor.h"
#include "../../jni/JNIBridgeFull.h"
#include "../widgets/IconSystem.h"
#include "imgui.h"
#include <cstring>
#include <algorithm>
#include <string>

static ImVec4 LogColor(LogLevel lv) {
    switch(lv) {
        case LogLevel::WARNING: return ForgeTheme::WARNING;
        case LogLevel::ERR:     return ForgeTheme::DANGER;
        case LogLevel::JME:     return ForgeTheme::ACCENT2;
        default:                return ForgeTheme::TEXT1;
    }
}
static const char* LogIcon(LogLevel lv) {
    switch(lv) {
        case LogLevel::WARNING: return u8"⚠";
        case LogLevel::ERR:     return u8"✖";
        case LogLevel::JME:     return u8"🔷";
        default:                return u8"ℹ";
    }
}

void RenderConsolePanel() {
    auto& e     = GEditor();
    auto& state = e.panelStates["console"];
    if (!state.open) return;

    ImGuiIO& io  = ImGui::GetIO();
    float pH     = state.minimized ? 28.f : 170.f;
    float pY     = io.DisplaySize.y - pH - 36.f;
    float lw     = 230.f;
    float rw     = 248.f;
    float pw     = io.DisplaySize.x - lw - rw;

    // Console lives right of hierarchy, left of inspector
    // (same row as project but offset when project also open)
    // For simplicity: full bottom when project is hidden
    bool projOpen = e.panelStates["project"].open;
    if (!projOpen) { pH = state.minimized ? 28.f : 200.f; }

    float consoleY = projOpen
        ? pY - (state.minimized ? 28.f : 200.f) - 4.f
        : pY;

    ImGui::SetNextWindowPos({lw, consoleY}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({pw, pH}, ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ForgeTheme::BG1);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f,0.f});
    ImGui::Begin("##console", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar);

    if (!ForgeUI::PanelHeader("##con_hdr","Console",u8">_", state)) {
        ImGui::End(); ImGui::PopStyleVar(); ImGui::PopStyleColor(); return;
    }
    if (state.minimized) {
        ImGui::End(); ImGui::PopStyleVar(); ImGui::PopStyleColor(); return;
    }

    // Toolbar
    static bool showInfo=true, showWarn=true, showErr=true, showJME=true;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ForgeTheme::BG2);
    ImGui::BeginChild("##con_tb",{0,26},false,ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos({4,3});

    auto FilterBtn = [](const char* icon, const char* lbl, bool& flag, ImVec4 col){
        ImGui::PushStyleColor(ImGuiCol_Button,
            flag ? ImVec4{col.x*0.25f,col.y*0.25f,col.z*0.25f,1.f}
                 : ForgeTheme::BG2);
        ImGui::PushStyleColor(ImGuiCol_Text, flag ? col : ForgeTheme::TEXT2);
        bool pressed = ImGui::Button((std::string(icon)+" "+lbl).c_str(),{0,20});
        if (pressed) flag = !flag;
        ImGui::PopStyleColor(2);
    };
    FilterBtn(u8"ℹ","Info",  showInfo, ForgeTheme::TEXT0);
    ImGui::SameLine(0,2);
    FilterBtn(u8"⚠","Warn",  showWarn, ForgeTheme::WARNING);
    ImGui::SameLine(0,2);
    FilterBtn(u8"✖","Error", showErr,  ForgeTheme::DANGER);
    ImGui::SameLine(0,2);
    FilterBtn(u8"🔷","JME",  showJME,  ForgeTheme::ACCENT2);
    ImGui::SameLine(0,8);

    static char consSearch[64]="";
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ForgeTheme::BG3);
    ImGui::SetNextItemWidth(100);
    ImGui::InputTextWithHint("##csrch",u8"Filter...",consSearch,64);
    ImGui::PopStyleColor();
    ImGui::SameLine(0,4);

    if (ForgeUI::SmallIconButton(u8"🗑","Clear console"))
        e.logs.clear();
    ImGui::SameLine(0,2);
    ImGui::Checkbox("Auto##autoscroll",&e.consoleAutoScroll);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Log list
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ForgeTheme::BG0);
    ImGui::BeginChild("##con_logs",{0,0},false);
    std::string filt(consSearch);

    for (auto& log : e.logs) {
        if (log.level==LogLevel::INFO    && !showInfo) continue;
        if (log.level==LogLevel::WARNING && !showWarn) continue;
        if (log.level==LogLevel::ERR     && !showErr)  continue;
        if (log.level==LogLevel::JME     && !showJME)  continue;
        if (!filt.empty() && log.msg.find(filt)==std::string::npos) continue;

        ImGui::PushStyleColor(ImGuiCol_Text, LogColor(log.level));
        ImGui::TextUnformatted(LogIcon(log.level));
        ImGui::PopStyleColor();
        ImGui::SameLine(0,4);
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT2);
        ImGui::TextUnformatted(log.time.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine(0,6);
        ImGui::PushStyleColor(ImGuiCol_Text, LogColor(log.level));
        ImGui::TextUnformatted(log.msg.c_str());
        ImGui::PopStyleColor();
    }
    if (e.consoleAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()-10)
        ImGui::SetScrollHereY(1.f);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End(); ImGui::PopStyleVar(); ImGui::PopStyleColor();
}

// ============================================================
//  AnimatorPanel.cpp  –  Animation timeline / clip manager
// ============================================================
void RenderAnimatorPanel() {
    auto& e     = GEditor();
    auto& state = e.panelStates["animator"];
    if (!state.open) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize({600,300}, ImGuiCond_Once);
    ImGui::SetNextWindowPos(
        {io.DisplaySize.x*0.5f-300, io.DisplaySize.y*0.5f-150},
        ImGuiCond_Once);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ForgeTheme::BG1);
    bool open = state.open;
    ImGui::Begin(u8"🎞  Animator##win", &open,
                 ImGuiWindowFlags_NoScrollbar);
    state.open = open;

    // Clip list (left)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ForgeTheme::BG2);
    ImGui::BeginChild("##anim_clips",{140,-1},true);
    ForgeUI::SectionHeader("Clips");
    for (int i = 0; i < (int)e.animClips.size(); i++) {
        auto& c = e.animClips[i];
        bool sel = (e.selectedClip == i);
        ImGui::PushStyleColor(ImGuiCol_Text,
            sel ? ForgeTheme::ACCENT : ForgeTheme::TEXT1);
        if (ImGui::Selectable(c.name.c_str(), sel, 0, {0,22}))
            e.selectedClip = i;
        ImGui::PopStyleColor();
    }
    ImGui::Spacing();
    if (ImGui::SmallButton(u8"＋ New Clip")) {
        AnimClip nc;
        nc.name = "Clip_"+std::to_string(e.animClips.size());
        e.animClips.push_back(nc);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine(0,4);

    // Timeline (right)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ForgeTheme::BG2);
    ImGui::BeginChild("##anim_timeline",{-1,-1},true);

    if (e.selectedClip >= 0 && e.selectedClip < (int)e.animClips.size()) {
        auto& clip = e.animClips[e.selectedClip];
        ForgeUI::SectionHeader(clip.name.c_str());

        // Controls row
        ImGui::SetNextItemWidth(80);
        ImGui::DragFloat("Length (s)##cl",&clip.length, 0.1f, 0.1f, 300.f);
        ImGui::SameLine(0,8);
        ImGui::SetNextItemWidth(60);
        ImGui::DragFloat("FPS##cl",&clip.fps, 1.f, 1.f, 120.f);
        ImGui::SameLine(0,8);
        ImGui::Checkbox("Loop##cl",&clip.loop);
        ImGui::SameLine(0,8);

        if (clip.playing) {
            if (ImGui::SmallButton(u8"⏸ Pause")) clip.playing=false;
        } else {
            if (ImGui::SmallButton(u8"▶ Play"))  clip.playing=true;
        }
        ImGui::SameLine(0,2);
        if (ImGui::SmallButton(u8"⏹")) { clip.playing=false; clip.cursor=0; }

        ImGui::Spacing();
        // Scrubber
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##cursor",&clip.cursor,0.f,clip.length,"%.2f s");

        ImGui::Spacing();
        // Timeline ruler (simplified visual)
        ImVec2 tlPos  = ImGui::GetCursorScreenPos();
        float  tlW    = ImGui::GetContentRegionAvail().x;
        float  tlH    = 80.f;
        auto*  dl     = ImGui::GetWindowDrawList();

        // Background
        dl->AddRectFilled(tlPos,{tlPos.x+tlW,tlPos.y+tlH},
            ImGui::ColorConvertFloat4ToU32(ForgeTheme::BG3), 4.f);

        // Track rows (mock)
        float trackH = 18.f;
        const char* tracks[] = {"Position","Rotation","Scale","Visibility"};
        for (int i = 0; i < 4; i++) {
            float ty = tlPos.y+4+i*(trackH+2);
            dl->AddRectFilled({tlPos.x,ty},{tlPos.x+60,ty+trackH},
                ImGui::ColorConvertFloat4ToU32(ForgeTheme::BG4));
            dl->AddText({tlPos.x+2,ty+2},
                ImGui::ColorConvertFloat4ToU32(ForgeTheme::TEXT2),tracks[i]);

            // Mock keyframes
            for (float kf : {0.1f, 0.4f, 0.7f, 0.95f}) {
                float kx = tlPos.x+60+(kf/clip.length)*(tlW-60);
                dl->AddCircleFilled({kx,ty+trackH/2},4.f,
                    ImGui::ColorConvertFloat4ToU32(ForgeTheme::ACCENT));
            }
        }
        // Cursor line
        float cx2 = tlPos.x+60+(clip.cursor/clip.length)*(tlW-60);
        dl->AddLine({cx2,tlPos.y},{cx2,tlPos.y+tlH},
            ImGui::ColorConvertFloat4ToU32(ForgeTheme::DANGER), 2.f);

        ImGui::Dummy({tlW,tlH});
        ImGui::Spacing();
        if (ImGui::SmallButton(u8"＋ Add Keyframe")) {}
        ImGui::SameLine(0,4);
        if (ImGui::SmallButton(u8"🗑 Delete Keyframe")) {}
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT2);
        ImGui::TextUnformatted("Select a clip from the left panel.");
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleColor();
}

// ============================================================
//  ViewportWindow.cpp  –  3D & 2D editor viewports
// ============================================================
void RenderViewport3D() {
    auto& e     = GEditor();
    ImGuiIO& io = ImGui::GetIO();

    float lw   = 230.f;
    float rw   = 248.f;
    float bh   = e.panelStates["project"].open ? 204.f : 0.f;
    float ch   = e.panelStates["console"].open ? 172.f : 0.f;
    float topH = 48.f;
    float vw   = io.DisplaySize.x - lw - rw;
    float vh   = io.DisplaySize.y - topH - std::max(bh,ch) - 36.f;

    ImGui::SetNextWindowPos({lw, topH}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({vw, vh}, ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.05f,0.06f,0.08f,1.f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f,0.f});
    ImGui::Begin("##viewport3d", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    // ── Viewport toolbar ─────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.06f,0.07f,0.09f,0.9f});
    ImGui::BeginChild("##vptb",{0,26},false,ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos({6,3});

    // Camera perspective toggle
    static bool persp = true;
    ImGui::PushStyleColor(ImGuiCol_Button, {0,0,0,0});
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT1);
    if (ImGui::SmallButton(persp?"Persp":"Ortho")) persp=!persp;
    ImGui::PopStyleColor(2);
    ImGui::SameLine(0,8);

    // View axis buttons
    for (auto& v : {"X","Y","Z","All"}) {
        ImGui::PushStyleColor(ImGuiCol_Button, {0,0,0,0});
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT2);
        ImGui::SmallButton(v);
        ImGui::PopStyleColor(2);
        ImGui::SameLine(0,2);
    }
    ImGui::SameLine(0,8);

    // Gizmo overlays
    ImGui::Checkbox(u8"Grid##vp",&e.viewportGrid);
    ImGui::SameLine(0,6);
    ImGui::Checkbox(u8"Gizmos##vp",&e.viewportGizmos);

    // Mode label (right)
    float mw = ImGui::CalcTextSize("3D Editor").x;
    ImGui::SameLine(ImGui::GetWindowWidth()-mw-30);
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT);
    ImGui::TextUnformatted(e.mode==EditorMode::MODE_3D?"3D Editor":"2D Editor");
    ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ── Viewport render area ──────────────────────────────────
    float vpContentH = ImGui::GetContentRegionAvail().y;
    float vpContentW = ImGui::GetContentRegionAvail().x;

    // JME renders here; show the texture from RenderPreviewFrame
    unsigned int texId = GJNI().RenderPreviewFrame((int)vpContentW,(int)vpContentH);
    if (texId) {
        ImGui::Image((ImTextureID)(uintptr_t)texId,
                     {vpContentW, vpContentH});
    } else {
        // Draw a placeholder grid when no texture
        ImVec2 vp0 = ImGui::GetCursorScreenPos();
        auto*  dl  = ImGui::GetWindowDrawList();
        if (e.viewportGrid) {
            float step = 40.f;
            ImU32 gc = IM_COL32(40,50,65,180);
            for (float x = vp0.x; x < vp0.x+vpContentW; x+=step)
                dl->AddLine({x,vp0.y},{x,vp0.y+vpContentH},gc);
            for (float y = vp0.y; y < vp0.y+vpContentH; y+=step)
                dl->AddLine({vp0.x,y},{vp0.x+vpContentW,y},gc);
            // Axis lines
            float cx3=vp0.x+vpContentW/2, cy3=vp0.y+vpContentH/2;
            dl->AddLine({cx3,vp0.y},{cx3,vp0.y+vpContentH},IM_COL32(60,60,120,200),2.f);
            dl->AddLine({vp0.x,cy3},{vp0.x+vpContentW,cy3},IM_COL32(120,60,60,200),2.f);
        }
        // "No renderer" label
        const char* hint = "JME Viewport";
        ImVec2 hs = ImGui::CalcTextSize(hint);
        dl->AddText({vp0.x+(vpContentW-hs.x)/2, vp0.y+(vpContentH-hs.y)/2},
            IM_COL32(80,100,130,200), hint);
        ImGui::Dummy({vpContentW,vpContentH});
    }

    // ── Overlay: selected object gizmo hint ──────────────────
    if (e.selectedNodeId >= 0 && e.viewportGizmos) {
        ImGui::GetWindowDrawList()->AddText(
            {ImGui::GetWindowPos().x+8,
             ImGui::GetWindowPos().y+vpContentH-20},
            ImGui::ColorConvertFloat4ToU32(ForgeTheme::ACCENT),
            "[W]Translate  [E]Rotate  [R]Scale  [Q]None");
    }

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void RenderViewport2D() {
    // 2D/UI editor – reuses same viewport slot with overlay
    // In a full implementation this would switch JME camera to orthographic
    // and show UI node handles.
}

// ============================================================
//  GamePreviewWindow.cpp  –  Floating play preview
// ============================================================
void RenderGamePreview() {
    auto& e     = GEditor();
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowSize(e.gamePreviewSize, ImGuiCond_Once);
    ImGui::SetNextWindowPos(
        {io.DisplaySize.x/2-e.gamePreviewSize.x/2,
         io.DisplaySize.y/2-e.gamePreviewSize.y/2},
        ImGuiCond_Once);
    ImGui::SetNextWindowSizeConstraints({200,150},{9999,9999});

    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0,0,0,1});
    bool open = e.showGamePreview;
    ImGui::Begin(u8"▶  Game Preview##gp", &open,
        ImGuiWindowFlags_NoScrollbar);
    e.showGamePreview = open;
    e.gamePreviewSize = ImGui::GetWindowSize();

    float pw = ImGui::GetContentRegionAvail().x;
    float ph = ImGui::GetContentRegionAvail().y;

    // Play state badge
    ImGui::SetCursorPos({6,4});
    const char* stateLabel =
        e.playState==PlayState::PLAYING ? u8"● PLAYING" :
        e.playState==PlayState::PAUSED  ? u8"⏸ PAUSED" : u8"⏹ STOPPED";
    ImVec4 stateCol =
        e.playState==PlayState::PLAYING ? ForgeTheme::ACCENT3 :
        e.playState==PlayState::PAUSED  ? ForgeTheme::WARNING :
        ForgeTheme::TEXT2;
    ForgeUI::Badge(stateLabel, stateCol);

    // Game render
    unsigned int texId = GJNI().RenderPreviewFrame((int)pw,(int)ph);
    if (texId) {
        ImGui::Image((ImTextureID)(uintptr_t)texId, {pw, ph});
    } else {
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            p0,{p0.x+pw,p0.y+ph},IM_COL32(5,8,12,255));
        ImGui::GetWindowDrawList()->AddText(
            {p0.x+pw/2-30,p0.y+ph/2},IM_COL32(60,80,100,255),"GAME VIEW");
        ImGui::Dummy({pw,ph});
    }

    // Bottom controls overlay
    ImVec2 cp = ImGui::GetWindowPos();
    float  ch2 = ImGui::GetWindowHeight();
    float  cw  = ImGui::GetWindowWidth();
    ImGui::SetCursorPos({cw/2-50, ch2-40});
    if (e.playState == PlayState::PLAYING) {
        if (ImGui::SmallButton(u8"⏸ Pause")) {
            e.playState = PlayState::PAUSED; GJNI().PauseGame();
        }
    } else {
        if (ImGui::SmallButton(u8"▶ Resume")) {
            e.playState = PlayState::PLAYING; GJNI().StartGame();
        }
    }
    ImGui::SameLine(0,4);
    if (ImGui::SmallButton(u8"⏹ Stop")) {
        e.playState=PlayState::STOPPED;
        e.showGamePreview=false;
        GJNI().StopGame();
    }

    ImGui::End();
    ImGui::PopStyleColor();
}

// ============================================================
//  SettingsWindow.cpp
// ============================================================
void RenderSettingsWindow() {
    auto& e = GEditor();
    ImGui::SetNextWindowSize({400,360},ImGuiCond_Once);
    ImGui::SetNextWindowPos(
        {ImGui::GetIO().DisplaySize.x/2-200,
         ImGui::GetIO().DisplaySize.y/2-180}, ImGuiCond_Once);

    bool open = e.showSettingsModal;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ForgeTheme::BG2);
    ImGui::Begin(u8"⚙  Settings", &open, ImGuiWindowFlags_NoScrollbar);
    e.showSettingsModal = open;

    static int tab = 0;
    const char* tabs[] = {"Editor","Rendering","Input","About"};
    for (int i = 0; i < 4; i++) {
        bool sel = (tab==i);
        ImGui::PushStyleColor(ImGuiCol_Button,
            sel ? ForgeTheme::BG4 : ForgeTheme::BG2);
        ImGui::PushStyleColor(ImGuiCol_Text,
            sel ? ForgeTheme::ACCENT : ForgeTheme::TEXT1);
        if (i>0) ImGui::SameLine(0,2);
        if (ImGui::Button(tabs[i],{90,28})) tab=i;
        ImGui::PopStyleColor(2);
    }
    ImGui::Separator();
    ImGui::Spacing();

    if (tab==0) {
        static float uiScale = 1.f;
        static bool darkMode = true;
        static bool autosave = true;
        static int  autosaveMin = 5;
        ImGui::SliderFloat("UI Scale",&uiScale,0.6f,2.f);
        ImGui::Checkbox("Dark Mode",&darkMode);
        ImGui::Checkbox("Autosave",&autosave);
        if (autosave) { ImGui::SameLine(0,8);
            ImGui::SetNextItemWidth(60);
            ImGui::DragInt("min##as",&autosaveMin,1,1,60); }
        ImGui::Checkbox("Show FPS counter",&e.viewportGizmos);
    }
    if (tab==1) {
        static bool shadows=true, postfx=true, aa=true;
        static int shadowRes=1; // 0=512 1=1024 2=2048
        static float renderScale=1.f;
        ImGui::Checkbox("Shadows",&shadows);
        if (shadows) {
            ImGui::SetNextItemWidth(140);
            ImGui::Combo("Shadow Res",&shadowRes,"512\0001024\0002048\000");
        }
        ImGui::Checkbox("Post FX",&postfx);
        ImGui::Checkbox("Anti-aliasing",&aa);
        ImGui::SliderFloat("Render Scale",&renderScale,0.5f,2.f);
    }
    if (tab==2) {
        static float touchSens = 1.f;
        static bool haptics = true;
        ImGui::SliderFloat("Touch Sensitivity",&touchSens,0.1f,3.f);
        ImGui::Checkbox("Haptic Feedback",&haptics);
    }
    if (tab==3) {
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT);
        ImGui::TextUnformatted("ForgeEngine v0.1.0");
        ImGui::PopStyleColor();
        ImGui::TextUnformatted("Built on JMonkeyEngine 3.x");
        ImGui::TextUnformatted("Editor: Dear ImGui");
        ImGui::TextUnformatted("Renderer: OpenGL ES 3.x");
    }

    ImGui::Spacing();
    if (ImGui::Button("Close",{80,28})) e.showSettingsModal=false;

    ImGui::End();
    ImGui::PopStyleColor();
}

// ============================================================
//  BuildWindow.cpp
// ============================================================
void RenderBuildWindow() {
    auto& e = GEditor();
    ImGui::SetNextWindowSize({420,300},ImGuiCond_Once);
    ImGui::SetNextWindowPos(
        {ImGui::GetIO().DisplaySize.x/2-210,
         ImGui::GetIO().DisplaySize.y/2-150}, ImGuiCond_Once);

    bool open = e.showBuildModal;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ForgeTheme::BG2);
    ImGui::Begin(u8"🔨  Build Project", &open);
    e.showBuildModal = open;

    static int platform = 0;
    static char outPath[128] = "/sdcard/ForgeBuilds/";
    static bool release = false;
    static int  buildProgress = -1;

    ImGui::TextUnformatted("Platform:");
    ImGui::RadioButton("Android APK",&platform,0); ImGui::SameLine(0,8);
    ImGui::RadioButton("Desktop",&platform,1);

    ImGui::Spacing();
    ImGui::TextUnformatted("Output path:");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ForgeTheme::BG3);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##outpath",outPath,128);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Checkbox("Release build (optimized)",&release);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    bool canBuild = (buildProgress < 0 || buildProgress >= 100);
    ImGui::BeginDisabled(!canBuild);
    ImGui::PushStyleColor(ImGuiCol_Button,
        {ForgeTheme::ACCENT3.x*0.3f,ForgeTheme::ACCENT3.y*0.3f,
         ForgeTheme::ACCENT3.z*0.3f,1.f});
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT3);
    if (ImGui::Button(u8"🔨  Build Now",{160,36})) {
        buildProgress = 0;
        GJNI().BuildProject(outPath,
            platform==0?"android":"desktop",
            [](int p, const std::string& msg){
                // Update progress from callback
            });
    }
    ImGui::PopStyleColor(2);
    ImGui::EndDisabled();

    if (buildProgress >= 0) {
        ImGui::SameLine(0,8);
        ImGui::ProgressBar((float)buildProgress/100.f,{-1,36});
        ImGui::TextColored(ForgeTheme::TEXT1,"Building...");
    }

    ImGui::Spacing();
    if (ImGui::Button("Cancel",{80,28})) e.showBuildModal=false;
    ImGui::End();
    ImGui::PopStyleColor();
}

// ============================================================
//  New Object popup (called from hierarchy panel)
// ============================================================
void RenderNewObjectPopup() {
    auto& e = GEditor();
    if (!e.showNewObjPopup) return;
    ImGui::SetNextWindowSize({260,360},ImGuiCond_Once);
    ImGui::SetNextWindowPos(
        {ImGui::GetIO().DisplaySize.x/2-130,
         ImGui::GetIO().DisplaySize.y/2-180}, ImGuiCond_Once);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ForgeTheme::BG2);
    bool open = e.showNewObjPopup;
    ImGui::Begin("Add Object##newobj", &open,
        ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoResize);
    e.showNewObjPopup = open;
    struct ObjEntry { const char* label; int type; };
    static const ObjEntry objs[] = {
        {"Node (Empty)",0},{"Box",1},{"Sphere",2},{"Cylinder",3},
        {"Capsule",4},{"Torus",5},{"Plane",6},{"Terrain",7},
        {"Point Light",8},{"Directional Light",9},{"Spot Light",10},
        {"Camera",11},{"Particle System",12}
    };
    ImGui::TextUnformatted("Select type to add:");
    ImGui::Separator(); ImGui::Spacing();
    ImGui::BeginChild("##objlist",{0,-36},false);
    for (auto& o : objs) {
        if (ImGui::Selectable(o.label)) {
            int pid = e.selectedNodeId;
            int nid = GJNI().AddSpatial((JMESpatialType)o.type, o.label, pid);
            if (nid > 0) { e.selectedNodeId = nid; GJNI().SyncSceneTree(); }
            e.showNewObjPopup = false;
        }
    }
    ImGui::EndChild();
    if (ImGui::Button("Cancel",{-1,28})) e.showNewObjPopup = false;
    ImGui::End();
    ImGui::PopStyleColor();
}

// ============================================================
//  About modal
// ============================================================
void RenderAboutModal() {
    auto& e = GEditor();
    ImGui::SetNextWindowSize({320,200},ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        {ImGui::GetIO().DisplaySize.x/2-160,
         ImGui::GetIO().DisplaySize.y/2-100}, ImGuiCond_Always);
    bool open = e.showAbout;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ForgeTheme::BG2);
    ImGui::Begin("About##ab",&open,
        ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove);
    e.showAbout = open;
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT);
    ImGui::SetWindowFontScale(1.4f);
    ImGui::TextUnformatted("⚡ ForgeEngine");
    ImGui::SetWindowFontScale(1.f);
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::TextWrapped("Mobile-first 3D/2D game editor built on JMonkeyEngine. "
                       "Create, edit, and build games directly on Android.");
    ImGui::Spacing();
    ImGui::TextColored(ForgeTheme::TEXT2,"v0.1.0 · JME 3.x · Dear ImGui");
    ImGui::Spacing();
    if (ImGui::Button("Close",{80,28})) e.showAbout=false;
    ImGui::End();
    ImGui::PopStyleColor();
}
