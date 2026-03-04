#pragma once
// ============================================================
//  ForgeEngine – IconSystem.h
//  Loads SVG/PNG icons from assets/icons/ into OpenGL textures.
//  Uses NanoSVG (header-only) or stb_image for PNG fallback.
//  All icon names map to: assets/icons/svg/<name>.svg
//                     or: assets/icons/png/<name>.png
// ============================================================
#include "imgui.h"
#include <string>
#include <unordered_map>

// ─── Icon enum – every entry has a matching SVG/PNG file ─────
enum class Icon {
    // ── General UI ──────────────────────────────────────────
    MENU,               // menu.svg
    CLOSE,              // close.svg
    MINIMIZE,           // minimize.svg
    MAXIMIZE,           // maximize.svg
    FLOAT,              // float.svg
    DOCK,               // dock.svg
    SEARCH,             // search.svg
    FILTER,             // filter.svg
    SETTINGS,           // settings.svg
    ADD,                // add.svg
    REMOVE,             // remove.svg
    SAVE,               // save.svg
    FOLDER_OPEN,        // folder_open.svg
    FOLDER_CLOSED,      // folder_closed.svg
    VISIBILITY_ON,      // visibility_on.svg
    VISIBILITY_OFF,     // visibility_off.svg
    BUILD,              // build.svg

    // ── Play controls ────────────────────────────────────────
    PLAY,               // play.svg
    PAUSE,              // pause.svg
    STOP,               // stop.svg

    // ── Edit actions ────────────────────────────────────────
    UNDO,               // undo.svg
    REDO,               // redo.svg

    // ── Gizmo / Transform ────────────────────────────────────
    GIZMO_TRANSLATE,    // gizmo_translate.svg
    GIZMO_ROTATE,       // gizmo_rotate.svg
    GIZMO_SCALE,        // gizmo_scale.svg
    SNAP,               // snap.svg
    COORD_LOCAL,        // coord_local.svg
    COORD_WORLD,        // coord_world.svg

    // ── Scene / Hierarchy objects ────────────────────────────
    NODE,               // node.svg
    GEOMETRY,           // geometry.svg
    LIGHT,              // light.svg
    CAMERA,             // camera.svg
    AUDIO,              // audio.svg
    TERRAIN,            // terrain.svg
    PARTICLE,           // particle.svg

    // ── Specific object types ────────────────────────────────
    OBJ_BOX,            // obj_box.svg
    OBJ_SPHERE,         // obj_sphere.svg
    OBJ_CYLINDER,       // obj_cylinder.svg
    OBJ_CAPSULE,        // obj_capsule.svg
    OBJ_TORUS,          // obj_torus.svg
    OBJ_PLANE,          // obj_plane.svg
    OBJ_LIGHT_POINT,    // obj_light_point.svg
    OBJ_LIGHT_DIR,      // obj_light_dir.svg
    OBJ_LIGHT_SPOT,     // obj_light_spot.svg

    // ── Asset types (Project panel) ──────────────────────────
    ASSET_SCENE,        // asset_scene.svg
    ASSET_MODEL,        // asset_model.svg
    ASSET_TEXTURE,      // asset_texture.svg
    ASSET_MATERIAL,     // asset_material.svg
    ASSET_AUDIO,        // asset_audio.svg
    ASSET_SCRIPT,       // asset_script.svg
    ASSET_PREFAB,       // asset_prefab.svg
    ASSET_UNKNOWN,      // asset_unknown.svg

    // ── Blueprint nodes ──────────────────────────────────────
    BP_EVENT,           // bp_event.svg
    BP_FUNCTION,        // bp_function.svg
    BP_VARIABLE,        // bp_variable.svg
    BP_FLOW,            // bp_flow.svg
    BP_MATH,            // bp_math.svg
    BP_LOGIC,           // bp_logic.svg
    BP_CAST,            // bp_cast.svg
    BP_MACRO,           // bp_macro.svg

    // ── Material editor nodes ────────────────────────────────
    MAT_OUTPUT,         // mat_output.svg
    MAT_TEXTURE,        // mat_texture.svg
    MAT_CONSTANT,       // mat_constant.svg
    MAT_MULTIPLY,       // mat_multiply.svg
    MAT_ADD,            // mat_add.svg

    // ── Code editor ─────────────────────────────────────────
    CODE_FILE,          // code_file.svg
    CODE_COMPILE,       // code_compile.svg
    CODE_RUN,           // code_run.svg

    // ── Prefab ──────────────────────────────────────────────
    PREFAB,             // prefab.svg
    PREFAB_APPLY,       // prefab_apply.svg
    PREFAB_BREAK,       // prefab_break.svg

    COUNT  // always last – used for array sizing
};

// ─── Icon size presets (pixels, at 1x DPI) ───────────────────
constexpr float ICON_SM  = 16.f;   // toolbar / list rows
constexpr float ICON_MD  = 22.f;   // panel headers / buttons
constexpr float ICON_LG  = 32.f;   // main toolbar
constexpr float ICON_XL  = 48.f;   // splash / large tiles

// ─── IconSystem singleton ────────────────────────────────────
class IconSystem {
public:
    static IconSystem& Get();

    // Call once after OpenGL ES context is ready.
    // basePath = internal storage path where icons/ was extracted.
    bool Init(const std::string& basePath);
    void Shutdown();

    // Generate minimal SVG fallbacks (plain colored squares) when
    // real icon files are missing – keeps editor usable always.
    void GenerateFallbackSVGs(const std::string& svgDir);

    // Returns ImTextureID (cast GL uint). Returns 0 if not loaded.
    ImTextureID GetTexID(Icon icon) const;

    // Draw icon at cursor position (cursor advances by size).
    void Draw(Icon icon, float size = ICON_MD,
              ImVec4 tint = {1.f,1.f,1.f,1.f}) const;

    // Invisible button with icon image – returns true if clicked.
    bool Button(Icon icon, const char* tooltip = nullptr,
                float size = ICON_MD,
                ImVec4 tint = {1.f,1.f,1.f,1.f}) const;

    // Icon on the left + text label to the right, full-width button.
    bool ButtonLabeled(Icon icon, const char* label,
                       float iconSize = ICON_MD) const;

    bool IsReady() const { return m_ready; }

private:
    struct Entry {
        unsigned int texID  = 0;
        int          width  = 0;
        int          height = 0;
    };

    Entry       m_icons[(int)Icon::COUNT] = {};
    bool        m_ready = false;
    std::string m_basePath;

    // Returns "menu" for MENU, "obj_box" for OBJ_BOX, etc.
    static const char* FileName(Icon icon);

    bool         LoadIcon(Icon icon);          // tries SVG then PNG
    unsigned int RasteriseSVG(const std::string& path,
                               int targetSize, int& w, int& h);
    unsigned int LoadPNG(const std::string& path, int& w, int& h);
    unsigned int UploadTexture(unsigned char* px, int w, int h);
};

// Convenience global
inline IconSystem& Icons() { return IconSystem::Get(); }
