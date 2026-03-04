#pragma once
// ============================================================
//  ForgeEngine – PrefabSystem.h
// ============================================================
#include <string>

struct SceneNode;

// Panel
void RenderPrefabPanel();

// Inspector section (called from InspectorPanel)
void DrawPrefabInspectorSection(SceneNode& node);

// Prefab data types
struct PrefabEntry {
    int         id       = 0;
    std::string name;
    std::string path;       // .prefab JSON file
    std::string typeName;   // e.g. "Crate", "Enemy"
    int         instances  = 0;
    bool        selected   = false;
};
