// ============================================================
//  ForgeEngine  –  IconSystem.cpp
//  SVG  → NanoSVG rasterizer → GL texture
//  PNG  → stb_image          → GL texture
// ============================================================

#include "IconSystem.h"
#include "imgui.h"

#include <GLES3/gl3.h>
#include <android/log.h>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cmath>

// ── Header-only libs (place in third_party/) ─────────────────
#define NANOSVG_IMPLEMENTATION
#define NANOSVG_ALL_COLOR_KEYWORDS
#include "nanosvg.h"           // third_party/nanosvg/nanosvg.h

#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"       // third_party/nanosvg/nanosvgrast.h

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"         // third_party/stb/stb_image.h

#define TAG "ForgeIcons"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// ════════════════════════════════════════════════════════════
//  Filename mapping  (must match enum order)
// ════════════════════════════════════════════════════════════
const char* IconSystem::FileName(Icon icon) {
    switch(icon) {
    // General UI
    case Icon::MENU:             return "menu";
    case Icon::CLOSE:            return "close";
    case Icon::MINIMIZE:         return "minimize";
    case Icon::MAXIMIZE:         return "maximize";
    case Icon::FLOAT:            return "float";
    case Icon::DOCK:             return "dock";
    case Icon::SEARCH:           return "search";
    case Icon::FILTER:           return "filter";
    case Icon::SETTINGS:         return "settings";
    case Icon::ADD:              return "add";
    case Icon::REMOVE:           return "remove";
    case Icon::SAVE:             return "save";
    case Icon::BUILD:            return "build";
    case Icon::FOLDER_OPEN:      return "folder_open";
    case Icon::FOLDER_CLOSED:    return "folder_closed";
    // Play
    case Icon::PLAY:             return "play";
    case Icon::PAUSE:            return "pause";
    case Icon::STOP:             return "stop";
    // Gizmo
    case Icon::GIZMO_TRANSLATE:  return "gizmo_translate";
    case Icon::GIZMO_ROTATE:     return "gizmo_rotate";
    case Icon::GIZMO_SCALE:      return "gizmo_scale";
    case Icon::SNAP:             return "snap";
    case Icon::UNDO:             return "undo";
    case Icon::REDO:             return "redo";
    case Icon::COORD_LOCAL:      return "coord_local";
    case Icon::COORD_WORLD:      return "coord_world";
    // Scene
    case Icon::NODE:             return "node";
    case Icon::GEOMETRY:         return "geometry";
    case Icon::LIGHT:            return "light";
    case Icon::CAMERA:           return "camera";
    case Icon::PARTICLE:         return "particle";
    case Icon::AUDIO:            return "audio";
    case Icon::TERRAIN:          return "terrain";
    case Icon::VISIBILITY_ON:    return "eye_open";
    case Icon::VISIBILITY_OFF:   return "eye_closed";
    // Objects
    case Icon::OBJ_BOX:          return "obj_box";
    case Icon::OBJ_SPHERE:       return "obj_sphere";
    case Icon::OBJ_CYLINDER:     return "obj_cylinder";
    case Icon::OBJ_CAPSULE:      return "obj_capsule";
    case Icon::OBJ_PLANE:        return "obj_plane";
    case Icon::OBJ_TORUS:        return "obj_torus";
    case Icon::OBJ_LIGHT_POINT:  return "obj_light_point";
    case Icon::OBJ_LIGHT_DIR:    return "obj_light_dir";
    case Icon::OBJ_LIGHT_SPOT:   return "obj_light_spot";
    // Assets
    case Icon::ASSET_MODEL:      return "asset_model";
    case Icon::ASSET_TEXTURE:    return "asset_texture";
    case Icon::ASSET_MATERIAL:   return "asset_material";
    case Icon::ASSET_AUDIO:      return "asset_audio";
    case Icon::ASSET_SCRIPT:     return "asset_script";
    case Icon::ASSET_SCENE:      return "asset_scene";
    case Icon::ASSET_PREFAB:     return "asset_prefab";
    case Icon::ASSET_UNKNOWN:    return "asset_file";
    // Blueprint
    case Icon::BP_EVENT:         return "bp_event";
    case Icon::BP_FUNCTION:      return "bp_function";
    case Icon::BP_VARIABLE:      return "bp_variable";
    case Icon::BP_MACRO:         return "bp_macro";
    case Icon::BP_FLOW:          return "bp_flow";
    case Icon::BP_MATH:          return "bp_math";
    case Icon::BP_LOGIC:         return "bp_logic";
    case Icon::BP_CAST:          return "bp_cast";
    // Material
    case Icon::MAT_TEXTURE:      return "mat_texture";
    case Icon::MAT_CONSTANT:     return "mat_constant";
    case Icon::MAT_MULTIPLY:     return "mat_multiply";
    case Icon::MAT_ADD:          return "mat_add";
    case Icon::MAT_OUTPUT:       return "mat_output";
    // Code
    case Icon::CODE_FILE:        return "code_file";
    case Icon::CODE_COMPILE:     return "code_compile";
    case Icon::CODE_RUN:         return "code_run";
    // Prefab
    case Icon::PREFAB:           return "prefab";
    case Icon::PREFAB_APPLY:     return "prefab_apply";
    case Icon::PREFAB_BREAK:     return "prefab_break";
    default:                     return "asset_file";
    }
}

// ════════════════════════════════════════════════════════════
//  Singleton
// ════════════════════════════════════════════════════════════
IconSystem& IconSystem::Get() {
    static IconSystem s;
    return s;
}

// ════════════════════════════════════════════════════════════
//  Init: load all icons
// ════════════════════════════════════════════════════════════
bool IconSystem::Init(const std::string& basePath) {
    m_basePath = basePath;
    int loaded = 0;
    for (int i = 0; i < (int)Icon::COUNT; i++) {
        if (LoadIcon((Icon)i)) loaded++;
    }
    LOGI("IconSystem: loaded %d / %d icons from %s",
         loaded, (int)Icon::COUNT, basePath.c_str());
    m_ready = true;
    return true;
}

void IconSystem::Shutdown() {
    for (auto& e : m_icons) {
        if (e.texID) glDeleteTextures(1, &e.texID);
        e.texID = 0;
    }
    m_ready = false;
}

// ════════════════════════════════════════════════════════════
//  Load one icon: SVG preferred, PNG fallback
// ════════════════════════════════════════════════════════════
bool IconSystem::LoadIcon(Icon icon) {
    const char* name = FileName(icon);
    auto& entry = m_icons[(int)icon];

    // Try SVG first
    std::string svgPath = m_basePath + "/svg/" + name + ".svg";
    int w=0, h=0;
    unsigned int tid = RasteriseSVG(svgPath, 64, w, h);
    if (tid) {
        entry = {tid, w, h};
        return true;
    }

    // Fallback: PNG
    std::string pngPath = m_basePath + "/png/" + name + ".png";
    tid = LoadPNG(pngPath, w, h);
    if (tid) {
        entry = {tid, w, h};
        return true;
    }

    // Generate a simple fallback texture (coloured square)
    static unsigned char fallback[4*4*4];
    memset(fallback, 0x44, sizeof(fallback));
    // Give it a colour based on icon category
    ImVec4 fc = {0.3f,0.5f,0.8f,1.f};
    if ((int)icon >= (int)Icon::BP_EVENT)    fc={0.3f,0.8f,0.5f,1.f};
    if ((int)icon >= (int)Icon::MAT_TEXTURE) fc={0.8f,0.4f,0.8f,1.f};
    for (int p=0; p<16; p++) {
        fallback[p*4+0]=(unsigned char)(fc.x*200);
        fallback[p*4+1]=(unsigned char)(fc.y*200);
        fallback[p*4+2]=(unsigned char)(fc.z*200);
        fallback[p*4+3]=200;
    }
    entry.texID = UploadTexture(fallback, 4, 4);
    entry.width = entry.height = 4;
    return false; // loaded fallback, not real icon
}

// ════════════════════════════════════════════════════════════
//  SVG rasteriser via NanoSVG
// ════════════════════════════════════════════════════════════
unsigned int IconSystem::RasteriseSVG(const std::string& path,
                                       int targetSize,
                                       int& outW, int& outH) {
    // Read file
    std::ifstream f(path, std::ios::binary);
    if (!f.good()) return 0;
    std::string src((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());
    if (src.empty()) return 0;

    // Parse SVG
    NSVGimage* img = nsvgParse((char*)src.c_str(), "px", 96.f);
    if (!img || img->width <= 0) { nsvgDelete(img); return 0; }

    // Scale to target size
    float scale = (float)targetSize / std::max(img->width, img->height);
    outW = (int)(img->width  * scale);
    outH = (int)(img->height * scale);
    if (outW<1) outW=1;
    if (outH<1) outH=1;

    // Rasterise
    NSVGrasterizer* rast = nsvgCreateRasterizer();
    std::vector<unsigned char> pixels((size_t)(outW * outH * 4), 0);
    nsvgRasterize(rast, img, 0, 0, scale,
                  pixels.data(), outW, outH, outW*4);
    nsvgDeleteRasterizer(rast);
    nsvgDelete(img);

    unsigned int tid = UploadTexture(pixels.data(), outW, outH);
    return tid;
}

// ════════════════════════════════════════════════════════════
//  PNG loader via stb_image
// ════════════════════════════════════════════════════════════
unsigned int IconSystem::LoadPNG(const std::string& path,
                                  int& outW, int& outH) {
    int channels;
    unsigned char* data = stbi_load(path.c_str(),
                                    &outW, &outH, &channels, 4);
    if (!data) return 0;
    unsigned int tid = UploadTexture(data, outW, outH);
    stbi_image_free(data);
    return tid;
}

// ════════════════════════════════════════════════════════════
//  Upload RGBA buffer to GL texture
// ════════════════════════════════════════════════════════════
unsigned int IconSystem::UploadTexture(unsigned char* pixels,
                                        int w, int h) {
    unsigned int tid;
    glGenTextures(1, &tid);
    glBindTexture(GL_TEXTURE_2D, tid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tid;
}

// ════════════════════════════════════════════════════════════
//  Draw / Button helpers
// ════════════════════════════════════════════════════════════
ImTextureID IconSystem::GetTexID(Icon icon) const {
    return (ImTextureID)(uintptr_t)m_icons[(int)icon].texID;
}

void IconSystem::Draw(Icon icon, float size, ImVec4 tint) const {
    ImTextureID tid = GetTexID(icon);
    if (tid)
        ImGui::Image(tid, {size, size}, {0,0},{1,1}, tint);
    else {
        // Fallback: coloured square placeholder
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            p, {p.x+size, p.y+size},
            IM_COL32(60,80,120,200), 3.f);
        ImGui::Dummy({size, size});
    }
}

bool IconSystem::Button(Icon icon, const char* tooltip,
                         float size, ImVec4 tint) const {
    ImGui::PushStyleColor(ImGuiCol_Button,        {0,0,0,0});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {1,1,1,0.08f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {1,1,1,0.15f});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {2.f,2.f});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);

    bool pressed = false;
    ImTextureID tid = GetTexID(icon);
    if (tid) {
        pressed = ImGui::ImageButton(
            (std::string("##icbtn")+(char*)FileName(icon)).c_str(),
            tid, {size,size}, {0,0},{1,1},
            {0,0,0,0}, tint);
    } else {
        pressed = ImGui::Button("?", {size+4, size+4});
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", tooltip);
    return pressed;
}

bool IconSystem::ButtonLabeled(Icon icon, const char* label,
                                float iconSize) const {
    ImGui::BeginGroup();
    Draw(icon, iconSize);
    ImGui::SameLine(0, 5);
    float ty = (iconSize - ImGui::GetTextLineHeight()) * 0.5f;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ty);
    bool hov = ImGui::IsItemHovered();
    if (hov) ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT);
    ImGui::TextUnformatted(label);
    if (hov) ImGui::PopStyleColor();
    ImGui::EndGroup();
    return ImGui::IsItemClicked();
}

// ════════════════════════════════════════════════════════════
//  SVG icon sources – generated at build time OR shipped
//  For development: generate minimal SVGs programmatically
// ════════════════════════════════════════════════════════════
// Call this to write all SVGs to assets/icons/svg/ if missing
void IconSystem::GenerateFallbackSVGs(const std::string& svgDir) {
    auto write = [&](const char* name, const char* body) {
        std::string path = svgDir + "/" + name + ".svg";
        std::ifstream chk(path);
        if (chk.good()) return; // already exists
        std::ofstream f(path);
        f << R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">)"
          << body << "</svg>";
    };

    // UI
    write("menu",           R"(<line x1="3" y1="6" x2="21" y2="6"/><line x1="3" y1="12" x2="21" y2="12"/><line x1="3" y1="18" x2="21" y2="18"/>)");
    write("close",          R"(<line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/>)");
    write("minimize",       R"(<line x1="5" y1="12" x2="19" y2="12"/>)");
    write("maximize",       R"(<rect x="3" y="3" width="18" height="18" rx="2"/>)");
    write("float",          R"(<rect x="4" y="7" width="14" height="10" rx="1"/><rect x="7" y="4" width="13" height="10" rx="1" stroke-dasharray="2"/>)");
    write("dock",           R"(<rect x="2" y="2" width="20" height="20" rx="2"/><line x1="2" y1="9" x2="22" y2="9"/>)");
    write("search",         R"(<circle cx="11" cy="11" r="7"/><line x1="21" y1="21" x2="16.65" y2="16.65"/>)");
    write("filter",         R"(<polygon points="22 3 2 3 10 12.46 10 19 14 21 14 12.46 22 3"/>)");
    write("settings",       R"(<circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83-2.83l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 2.83-2.83l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 2.83l-.06.06A1.65 1.65 0 0 0 19.4 9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z"/>)");
    write("add",            R"(<circle cx="12" cy="12" r="9"/><line x1="12" y1="8" x2="12" y2="16"/><line x1="8" y1="12" x2="16" y2="12"/>)");
    write("remove",         R"(<circle cx="12" cy="12" r="9"/><line x1="8" y1="12" x2="16" y2="12"/>)");
    write("save",           R"(<path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"/><polyline points="17 21 17 13 7 13 7 21"/><polyline points="7 3 7 8 15 8"/>)");
    write("folder_open",    R"(<path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"/><line x1="12" y1="11" x2="12" y2="17"/><line x1="9" y1="14" x2="15" y2="14"/>)");
    write("folder_closed",  R"(<path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"/>)");
    // Play controls
    write("play",           R"(<polygon points="5 3 19 12 5 21 5 3" fill="currentColor"/>)");
    write("pause",          R"(<rect x="6" y="4" width="4" height="16" fill="currentColor"/><rect x="14" y="4" width="4" height="16" fill="currentColor"/>)");
    write("stop",           R"(<rect x="4" y="4" width="16" height="16" rx="1" fill="currentColor"/>)");
    // Gizmos
    write("gizmo_translate",R"(<line x1="12" y1="19" x2="12" y2="5"/><polyline points="5 12 12 5 19 12"/><line x1="5" y1="19" x2="19" y2="19"/>)");
    write("gizmo_rotate",   R"(<polyline points="23 4 23 10 17 10"/><path d="M20.49 15a9 9 0 1 1-2.12-9.36L23 10"/>)");
    write("gizmo_scale",    R"(<polyline points="15 3 21 3 21 9"/><line x1="10" y1="14" x2="21" y2="3"/><polyline points="9 21 3 21 3 15"/><line x1="14" y1="10" x2="3" y2="21"/>)");
    write("snap",           R"(<path d="M12 2L2 7l10 5 10-5-10-5z"/><path d="M2 17l10 5 10-5"/><path d="M2 12l10 5 10-5"/>)");
    write("undo",           R"(<polyline points="9 14 4 9 9 4"/><path d="M20 20v-7a4 4 0 0 0-4-4H4"/>)");
    write("redo",           R"(<polyline points="15 14 20 9 15 4"/><path d="M4 20v-7a4 4 0 0 0 4-4h12"/>)");
    write("coord_local",    R"(<circle cx="12" cy="12" r="3"/><line x1="12" y1="3" x2="12" y2="9"/><line x1="12" y1="15" x2="12" y2="21"/><line x1="3" y1="12" x2="9" y2="12"/><line x1="15" y1="12" x2="21" y2="12"/>)");
    write("coord_world",    R"(<circle cx="12" cy="12" r="9"/><line x1="3.6" y1="9" x2="20.4" y2="9"/><line x1="3.6" y1="15" x2="20.4" y2="15"/><line x1="12" y1="3" x2="12" y2="21"/><path d="M11.5 3a17 17 0 0 0 0 18M12.5 3a17 17 0 0 1 0 18"/>)");
    // Scene
    write("node",           R"(<rect x="9" y="3" width="6" height="4" rx="1"/><rect x="1" y="17" width="6" height="4" rx="1"/><rect x="9" y="17" width="6" height="4" rx="1"/><rect x="17" y="17" width="6" height="4" rx="1"/><line x1="12" y1="7" x2="12" y2="12"/><line x1="12" y1="12" x2="4" y2="19"/><line x1="12" y1="12" x2="12" y2="19"/><line x1="12" y1="12" x2="20" y2="19"/>)");
    write("geometry",       R"(<polygon points="12 2 22 8.5 22 15.5 12 22 2 15.5 2 8.5 12 2"/>)");
    write("light",          R"(<circle cx="12" cy="12" r="4"/><line x1="12" y1="2" x2="12" y2="4"/><line x1="12" y1="20" x2="12" y2="22"/><line x1="4.22" y1="4.22" x2="5.64" y2="5.64"/><line x1="18.36" y1="18.36" x2="19.78" y2="19.78"/><line x1="2" y1="12" x2="4" y2="12"/><line x1="20" y1="12" x2="22" y2="12"/><line x1="4.22" y1="19.78" x2="5.64" y2="18.36"/><line x1="18.36" y1="5.64" x2="19.78" y2="4.22"/>)");
    write("camera",         R"(<path d="M23 7l-7 5 7 5V7z"/><rect x="1" y="5" width="15" height="14" rx="2" ry="2"/>)");
    write("particle",       R"(<circle cx="12" cy="12" r="2"/><circle cx="6" cy="7" r="1.5"/><circle cx="18" cy="7" r="1.5"/><circle cx="6" cy="17" r="1.5"/><circle cx="18" cy="17" r="1.5"/><circle cx="12" cy="4" r="1"/><circle cx="12" cy="20" r="1"/>)");
    write("audio",          R"(<polygon points="11 5 6 9 2 9 2 15 6 15 11 19 11 5"/><path d="M19.07 4.93a10 10 0 0 1 0 14.14M15.54 8.46a5 5 0 0 1 0 7.07"/>)");
    write("terrain",        R"(<polygon points="2 20 7 8 12 14 16 6 22 20 2 20" fill="currentColor" opacity="0.4"/><polyline points="2 20 7 8 12 14 16 6 22 20"/>)");
    write("eye_open",       R"(<path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/>)");
    write("eye_closed",     R"(<path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94"/><path d="M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19"/><line x1="1" y1="1" x2="23" y2="23"/>)");
    // Objects
    write("obj_box",        R"(<path d="M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z"/>)");
    write("obj_sphere",     R"(<circle cx="12" cy="12" r="9"/><ellipse cx="12" cy="12" rx="9" ry="4"/><line x1="12" y1="3" x2="12" y2="21"/>)");
    write("obj_cylinder",   R"(<ellipse cx="12" cy="6" rx="8" ry="3"/><line x1="4" y1="6" x2="4" y2="18"/><line x1="20" y1="6" x2="20" y2="18"/><ellipse cx="12" cy="18" rx="8" ry="3"/>)");
    write("obj_capsule",    R"(<path d="M8 12V8a4 4 0 0 1 8 0v4"/><rect x="8" y="12" width="8" height="4"/><path d="M8 16v1a4 4 0 0 0 8 0v-1"/>)");
    write("obj_plane",      R"(<polygon points="2 20 12 14 22 20 12 8 2 20" opacity="0.4" fill="currentColor"/><polyline points="2 20 12 14 22 20"/>)");
    write("obj_torus",      R"(<circle cx="12" cy="12" r="9"/><circle cx="12" cy="12" r="4"/>)");
    write("obj_light_point",R"(<circle cx="12" cy="12" r="4"/><line x1="12" y1="2" x2="12" y2="5"/><line x1="12" y1="19" x2="12" y2="22"/><line x1="4.22" y1="4.22" x2="6.34" y2="6.34"/><line x1="17.66" y1="17.66" x2="19.78" y2="19.78"/><line x1="2" y1="12" x2="5" y2="12"/><line x1="19" y1="12" x2="22" y2="12"/>)");
    write("obj_light_dir",  R"(<line x1="12" y1="2" x2="12" y2="22"/><line x1="6" y1="4" x2="18" y2="4"/><line x1="4" y1="8" x2="20" y2="8"/><line x1="2" y1="12" x2="22" y2="12"/><line x1="4" y1="16" x2="20" y2="16"/><line x1="6" y1="20" x2="18" y2="20"/>)");
    write("obj_light_spot", R"(<line x1="12" y1="2" x2="12" y2="9"/><path d="M6.5 17.5l-2 2.5M17.5 17.5l2 2.5"/><path d="M7 13l-3 7h16l-3-7H7z"/>)");
    // Assets
    write("asset_model",    R"(<path d="M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z"/><polyline points="3.27 6.96 12 12.01 20.73 6.96"/><line x1="12" y1="22.08" x2="12" y2="12"/>)");
    write("asset_texture",  R"(<rect x="3" y="3" width="18" height="18" rx="2"/><circle cx="8.5" cy="8.5" r="1.5"/><polyline points="21 15 16 10 5 21"/>)");
    write("asset_material", R"(<circle cx="12" cy="12" r="9"/><path d="M12 3a4.5 4.5 0 0 0 0 9 4.5 4.5 0 0 0 0 9"/>)");
    write("asset_audio",    R"(<path d="M9 18V5l12-2v13"/><circle cx="6" cy="18" r="3"/><circle cx="18" cy="16" r="3"/>)");
    write("asset_script",   R"(<polyline points="16 18 22 12 16 6"/><polyline points="8 6 2 12 8 18"/>)");
    write("asset_scene",    R"(<rect x="3" y="3" width="18" height="18" rx="2"/><circle cx="9" cy="9" r="2"/><path d="M21 15l-5-5L5 21"/>)");
    write("asset_prefab",   R"(<polygon points="12 2 15.09 8.26 22 9.27 17 14.14 18.18 21.02 12 17.77 5.82 21.02 7 14.14 2 9.27 8.91 8.26 12 2" fill="currentColor" opacity="0.6"/>)");
    write("asset_file",     R"(<path d="M13 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V9z"/><polyline points="13 2 13 9 20 9"/>)");
    // Blueprint
    write("bp_event",       R"(<polygon points="13 2 3 14 12 14 11 22 21 10 12 10 13 2" fill="currentColor"/>)");
    write("bp_function",    R"(<path d="M4 9h16"/><path d="M4 15h16"/><path d="M10 3L8 21"/><path d="M16 3l-2 18"/>)");
    write("bp_variable",    R"(<path d="M20.59 13.41l-7.17 7.17a2 2 0 0 1-2.83 0L2 12V2h10l8.59 8.59a2 2 0 0 1 0 2.82z"/><line x1="7" y1="7" x2="7.01" y2="7"/>)");
    write("bp_macro",       R"(<path d="M5 3l14 9-14 9V3z" fill="currentColor" opacity="0.5"/><path d="M5 3l14 9-14 9V3z"/>)");
    write("bp_flow",        R"(<line x1="5" y1="12" x2="19" y2="12"/><polyline points="12 5 19 12 12 19"/>)");
    write("bp_math",        R"(<line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/>)");
    write("bp_logic",       R"(<path d="M18 16.08c-.76.09-1.48.29-2.12.69a4.12 4.12 0 0 0-1.74 2.08 4 4 0 0 1-3.28-5.21L4 4"/><path d="M20 4L8.12 15.88M14.47 14.48L20 20M4 20l6-6"/>)");
    write("bp_cast",        R"(<polyline points="17 1 21 5 17 9"/><path d="M3 11V9a4 4 0 0 1 4-4h14"/><polyline points="7 23 3 19 7 15"/><path d="M21 13v2a4 4 0 0 1-4 4H3"/>)");
    // Material
    write("mat_texture",    R"(<rect x="3" y="3" width="18" height="18" rx="2"/><circle cx="8.5" cy="8.5" r="1.5"/><polyline points="21 15 16 10 5 21"/>)");
    write("mat_constant",   R"(<rect x="6" y="6" width="12" height="12" rx="2" fill="currentColor"/>)");
    write("mat_multiply",   R"(<line x1="5" y1="5" x2="19" y2="19"/><line x1="19" y1="5" x2="5" y2="19"/>)");
    write("mat_add",        R"(<line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/>)");
    write("mat_output",     R"(<circle cx="12" cy="12" r="9"/><circle cx="12" cy="12" r="4" fill="currentColor"/>)");
    // Code
    write("code_file",      R"(<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/><line x1="8" y1="13" x2="16" y2="13"/><line x1="8" y1="17" x2="13" y2="17"/>)");
    write("code_compile",   R"(<polyline points="16 18 22 12 16 6"/><polyline points="8 6 2 12 8 18"/><line x1="19" y1="12" x2="5" y2="12"/>)");
    write("code_run",       R"(<circle cx="12" cy="12" r="9"/><polygon points="10 8 16 12 10 16 10 8" fill="currentColor"/>)");
    // Prefab
    write("prefab",         R"(<polygon points="12 2 15.09 8.26 22 9.27 17 14.14 18.18 21.02 12 17.77 5.82 21.02 7 14.14 2 9.27 8.91 8.26 12 2" fill="currentColor" opacity="0.7"/>)");
    write("prefab_apply",   R"(<polyline points="20 6 9 17 4 12"/><circle cx="12" cy="12" r="9" stroke-dasharray="4"/>)");
    write("prefab_break",   R"(<line x1="4" y1="4" x2="20" y2="20"/><circle cx="12" cy="12" r="9"/>)");

    LOGI("Generated fallback SVGs in %s", svgDir.c_str());
}
