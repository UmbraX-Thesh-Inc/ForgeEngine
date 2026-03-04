// ============================================================
//  ForgeEngine  –  ForgeEditor.cpp
//  Editor core: init, main loop, theme, UI helpers, globals
// ============================================================

#include "ForgeEditor.h"
#include "../jni/JNIBridgeFull.h"
#include "panels/AllPanels.h"
#include "imgui.h"
#include "imgui_internal.h"

// ─── Globals ─────────────────────────────────────────────────
// GEditor() is defined here. GJNI() is defined in jni/JNIBridgeFull.cpp.
static ForgeEditorState g_editor;

ForgeEditorState& GEditor() { return g_editor; }

// ════════════════════════════════════════════════════════════
//  THEME
// ════════════════════════════════════════════════════════════
void ForgeTheme::Apply() {
    ImGuiStyle& s = ImGui::GetStyle();

    // Rounding
    s.WindowRounding    = ROUNDING;
    s.ChildRounding     = ROUNDING;
    s.FrameRounding     = ROUNDING_SM;
    s.PopupRounding     = ROUNDING;
    s.ScrollbarRounding = ROUNDING;
    s.GrabRounding      = ROUNDING_SM;
    s.TabRounding       = ROUNDING_SM;

    // Spacing
    s.WindowPadding     = {10, 8};
    s.FramePadding      = {8,  5};
    s.ItemSpacing       = {6,  5};
    s.ItemInnerSpacing  = {5,  4};
    s.ScrollbarSize     = 7;
    s.GrabMinSize       = 8;
    s.WindowBorderSize  = 1;
    s.ChildBorderSize   = 1;
    s.TabBorderSize     = 0;
    s.IndentSpacing     = 14;

    // Touch-friendly: bigger hit targets
    s.TouchExtraPadding = {3, 3};
    s.LogSliderDeadzone = 6;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]             = BG1;
    c[ImGuiCol_ChildBg]              = BG0;
    c[ImGuiCol_PopupBg]              = {BG2.x, BG2.y, BG2.z, 0.97f};
    c[ImGuiCol_Border]               = BORDER;
    c[ImGuiCol_BorderShadow]         = {0,0,0,0};

    c[ImGuiCol_FrameBg]              = BG3;
    c[ImGuiCol_FrameBgHovered]       = BG4;
    c[ImGuiCol_FrameBgActive]        = {ACCENT.x*0.3f, ACCENT.y*0.3f, ACCENT.z*0.3f, 1.f};

    c[ImGuiCol_TitleBg]              = BG0;
    c[ImGuiCol_TitleBgActive]        = BG2;
    c[ImGuiCol_TitleBgCollapsed]     = BG0;

    c[ImGuiCol_MenuBarBg]            = BG1;

    c[ImGuiCol_ScrollbarBg]          = BG0;
    c[ImGuiCol_ScrollbarGrab]        = BG4;
    c[ImGuiCol_ScrollbarGrabHovered] = {ACCENT.x*0.5f, ACCENT.y*0.5f, ACCENT.z*0.5f, 1.f};
    c[ImGuiCol_ScrollbarGrabActive]  = ACCENT;

    c[ImGuiCol_CheckMark]            = ACCENT;
    c[ImGuiCol_SliderGrab]           = ACCENT;
    c[ImGuiCol_SliderGrabActive]     = {ACCENT.x*1.2f, ACCENT.y*1.2f, ACCENT.z*1.2f, 1.f};

    c[ImGuiCol_Button]               = BG3;
    c[ImGuiCol_ButtonHovered]        = BG4;
    c[ImGuiCol_ButtonActive]         = {ACCENT.x*0.4f, ACCENT.y*0.4f, ACCENT.z*0.4f, 1.f};

    c[ImGuiCol_Header]               = {ACCENT.x*0.15f, ACCENT.y*0.15f, ACCENT.z*0.15f, 1.f};
    c[ImGuiCol_HeaderHovered]        = {ACCENT.x*0.25f, ACCENT.y*0.25f, ACCENT.z*0.25f, 1.f};
    c[ImGuiCol_HeaderActive]         = {ACCENT.x*0.35f, ACCENT.y*0.35f, ACCENT.z*0.35f, 1.f};

    c[ImGuiCol_Separator]            = BORDER;
    c[ImGuiCol_SeparatorHovered]     = BORDER_HL;
    c[ImGuiCol_SeparatorActive]      = ACCENT;

    c[ImGuiCol_ResizeGrip]           = {ACCENT.x*0.3f, ACCENT.y*0.3f, ACCENT.z*0.3f, 0.5f};
    c[ImGuiCol_ResizeGripHovered]    = ACCENT;
    c[ImGuiCol_ResizeGripActive]     = ACCENT;

    c[ImGuiCol_Tab]                  = BG2;
    c[ImGuiCol_TabHovered]           = BG4;
    c[ImGuiCol_TabActive]            = {ACCENT.x*0.25f, ACCENT.y*0.25f, ACCENT.z*0.25f, 1.f};
    c[ImGuiCol_TabUnfocused]         = BG1;
    c[ImGuiCol_TabUnfocusedActive]   = BG3;

    c[ImGuiCol_DockingPreview]       = {ACCENT.x, ACCENT.y, ACCENT.z, 0.3f};
    c[ImGuiCol_DockingEmptyBg]       = BG0;

    c[ImGuiCol_PlotLines]            = ACCENT3;
    c[ImGuiCol_PlotHistogram]        = ACCENT;

    c[ImGuiCol_TableHeaderBg]        = BG2;
    c[ImGuiCol_TableBorderStrong]    = BORDER;
    c[ImGuiCol_TableBorderLight]     = {BORDER.x, BORDER.y, BORDER.z, 0.04f};

    c[ImGuiCol_Text]                 = TEXT0;
    c[ImGuiCol_TextDisabled]         = TEXT2;
    c[ImGuiCol_TextSelectedBg]       = {ACCENT.x*0.3f, ACCENT.y*0.3f, ACCENT.z*0.3f, 0.5f};

    c[ImGuiCol_NavHighlight]         = ACCENT;
    c[ImGuiCol_NavWindowingHighlight]= ACCENT;
}

// ════════════════════════════════════════════════════════════
//  ForgeUI helpers
// ════════════════════════════════════════════════════════════
namespace ForgeUI {

bool IconButton(const char* icon, const char* tooltip,
                ImVec4 col, float size) {
    ImGui::PushStyleColor(ImGuiCol_Button,        {col.x*0.15f,col.y*0.15f,col.z*0.15f,1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {col.x*0.30f,col.y*0.30f,col.z*0.30f,1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {col.x*0.50f,col.y*0.50f,col.z*0.50f,1.f});
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    bool pressed = ImGui::Button(icon, {size, size});
    ImGui::PopStyleColor(4);
    if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", tooltip);
    return pressed;
}

bool SmallIconButton(const char* icon, const char* tooltip) {
    return IconButton(icon, tooltip, ForgeTheme::ACCENT, 22.f);
}

void Separator() {
    ImGui::PushStyleColor(ImGuiCol_Separator, ForgeTheme::BORDER);
    ImGui::Separator();
    ImGui::PopStyleColor();
}

void SectionHeader(const char* label) {
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT2);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Separator, ForgeTheme::BORDER);
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

bool PanelHeader(const char* id, const char* label, const char* icon,
                 PanelState& state) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ForgeTheme::BG2);
    ImGui::BeginChild(id, {0, 28}, false, ImGuiWindowFlags_NoScrollbar);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY()+4);
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT);
    ImGui::TextUnformatted(icon);
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 6);
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT0);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();

    // Right-side buttons
    float bw = 22.f;
    float rx  = ImGui::GetWindowWidth() - bw*2 - 10;
    ImGui::SameLine(rx);
    bool minimized = state.minimized;
    if (SmallIconButton(minimized ? u8"+" : u8"−")) state.minimized = !state.minimized;
    ImGui::SameLine(0,2);
    if (SmallIconButton(u8"✕")) state.open = false;

    ImGui::EndChild();
    ImGui::PopStyleColor();
    return state.open && !state.minimized;
}

void TooltipIfHovered(const char* tip) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", tip);
}

void Badge(const char* text, ImVec4 col) {
    ImVec2 p  = ImGui::GetCursorScreenPos();
    ImVec2 sz = ImGui::CalcTextSize(text);
    float pad = 4.f;
    ImGui::GetWindowDrawList()->AddRectFilled(
        {p.x-pad, p.y-1.f},
        {p.x+sz.x+pad, p.y+sz.y+1.f},
        ImGui::ColorConvertFloat4ToU32(col), 3.f);
    ImGui::TextColored({1,1,1,1}, "%s", text);
}

bool DragFloat3Compact(const char* id, float* v, float speed) {
    bool changed = false;
    float fw = (ImGui::GetContentRegionAvail().x - 8) / 3.f;
    ImGui::PushItemWidth(fw);
    ImGui::PushStyleColor(ImGuiCol_Text, {0.9f, 0.3f, 0.3f, 1.f});
    changed |= ImGui::DragFloat((std::string("##x")+id).c_str(), &v[0], speed);
    ImGui::PopStyleColor();
    ImGui::SameLine(0,4);
    ImGui::PushStyleColor(ImGuiCol_Text, {0.3f, 0.9f, 0.3f, 1.f});
    changed |= ImGui::DragFloat((std::string("##y")+id).c_str(), &v[1], speed);
    ImGui::PopStyleColor();
    ImGui::SameLine(0,4);
    ImGui::PushStyleColor(ImGuiCol_Text, {0.3f, 0.5f, 1.0f, 1.f});
    changed |= ImGui::DragFloat((std::string("##z")+id).c_str(), &v[2], speed);
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();
    return changed;
}

bool DragFloat3CompactLabeled(const char* label, float* v, float speed) {
    float lw = 70.f;
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT1);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::SameLine(lw);
    return DragFloat3Compact(label, v, speed);
}

} // namespace ForgeUI

// ForgeEditor_Render and ForgeEditor_Init are defined in ForgeEditor_Main.cpp
