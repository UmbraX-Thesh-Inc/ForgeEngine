#pragma once
// ============================================================
//  ForgeEngine – AllPanels.h
//  Master declarations for all panels and windows.
// ============================================================

// ── Main panels ──────────────────────────────────────────────
void RenderTopBar();
void RenderHierarchyPanel();
void RenderInspectorPanel();
void RenderInspectorPanelFull();
void RenderProjectPanel();
void RenderConsolePanel();
void RenderAnimatorPanel();

// ── Windows (all implemented in EditorPanels.cpp) ────────────
void RenderViewport3D();
void RenderViewport2D();
void RenderGamePreview();
void RenderSettingsWindow();
void RenderBuildWindow();
void RenderNewObjectPopup();
void RenderAboutModal();

// ── Full-screen editors ──────────────────────────────────────
void RenderBlueprintEditor();
void RenderMaterialEditor();
void RenderCodeEditor();
void RenderPrefabPanel();
void RenderNetworkPanel();

// ── Gizmo (called from viewport) ─────────────────────────────
void RenderTransformGizmo(ImDrawList* dl, ImVec2 viewPos, ImVec2 viewSize);

// ── Prefab inspector section ──────────────────────────────────
struct SceneNode;
void DrawPrefabInspectorSection(SceneNode& node);
