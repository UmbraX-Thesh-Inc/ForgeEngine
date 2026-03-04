// ============================================================
//  ForgeEngine  –  TransformGizmo.cpp
//  Interactive 3D transform gizmos drawn directly with ImGui
//  DrawList + projected math (no separate gizmo library needed).
//  Supports: Translate (arrows), Rotate (rings), Scale (cubes)
//  Mobile-friendly: large touch hit areas
// ============================================================

#include "../ForgeEditor.h"
#include "../../jni/JNIBridgeFull.h"
#include "imgui.h"
#include <cmath>
#include <algorithm>

// ─── Minimal 3D math structs ─────────────────────────────────
struct Vec3 { float x,y,z; };
struct Vec4 { float x,y,z,w; };
struct Mat4 { float m[16]; };

static Vec3 operator+(Vec3 a, Vec3 b){ return {a.x+b.x,a.y+b.y,a.z+b.z}; }
static Vec3 operator-(Vec3 a, Vec3 b){ return {a.x-b.x,a.y-b.y,a.z-b.z}; }
static Vec3 operator*(Vec3 a, float s){ return {a.x*s,a.y*s,a.z*s}; }
static float dot(Vec3 a, Vec3 b){ return a.x*b.x+a.y*b.y+a.z*b.z; }
static float len(Vec3 v){ return sqrtf(dot(v,v)); }
static Vec3  norm(Vec3 v){ float l=len(v); return l>0?v*(1.f/l):v; }

// ─── Project world point → screen ────────────────────────────
// Camera matrices passed from the viewport (simplified perspective)
struct GizmoCamera {
    float viewProj[16];  // column-major MVP
    ImVec2 viewPos;
    ImVec2 viewSize;
};

static ImVec2 WorldToScreen(Vec3 wp, const GizmoCamera& cam) {
    float* m = (float*)cam.viewProj;
    // Homogeneous clip
    float cx = m[0]*wp.x + m[4]*wp.y + m[8]*wp.z  + m[12];
    float cy = m[1]*wp.x + m[5]*wp.y + m[9]*wp.z  + m[13];
    float cz = m[2]*wp.x + m[6]*wp.y + m[10]*wp.z + m[14];
    float cw = m[3]*wp.x + m[7]*wp.y + m[11]*wp.z + m[15];
    if (fabsf(cw) < 0.0001f) return {-9999,-9999};
    float nx = cx/cw, ny = cy/cw;
    return {
        cam.viewPos.x + (nx*0.5f+0.5f)*cam.viewSize.x,
        cam.viewPos.y + (-ny*0.5f+0.5f)*cam.viewSize.y
    };
}

// ─── Axis colours ─────────────────────────────────────────────
static ImU32 AxisColor(int axis, bool hovered=false, bool active=false) {
    if (active)  return IM_COL32(255,220,50,255);
    if (hovered) return IM_COL32(255,255,100,200);
    switch(axis) {
    case 0: return IM_COL32(220,60,60,220);   // X = red
    case 1: return IM_COL32(80,200,80,220);   // Y = green
    case 2: return IM_COL32(60,120,220,220);  // Z = blue
    }
    return IM_COL32(200,200,200,200);
}

// ─── Gizmo state ──────────────────────────────────────────────
struct GizmoState {
    int   hoveredAxis   = -1;
    int   activeAxis    = -1;
    ImVec2 dragStart    = {0,0};
    float  dragStartVal[3] = {0,0,0};
    bool   dragging     = false;
    // Screen-space axis directions (set per frame)
    ImVec2 axisDirSS[3];
    // Size on screen (scales with distance)
    float  screenSize   = 90.f;
};

static GizmoState g_gizmo;

// ════════════════════════════════════════════════════════════
//  Build a simple perspective view-projection matrix
// ════════════════════════════════════════════════════════════
// In a real implementation this comes from the JME camera.
// Here we build a test matrix for the editor viewport.
static void BuildViewProj(GizmoCamera& cam,
                           Vec3 eye, Vec3 target, Vec3 up,
                           float fovY, float aspect,
                           float nearZ, float farZ) {
    // View matrix (LookAt)
    Vec3 f = norm(target - eye);
    Vec3 r = norm({f.y*up.z-f.z*up.y, f.z*up.x-f.x*up.z, f.x*up.y-f.y*up.x});
    Vec3 u2 = {r.y*f.z-r.z*f.y, r.z*f.x-r.x*f.z, r.x*f.y-r.y*f.x};

    float view[16] = {
         r.x,  u2.x, -f.x, 0,
         r.y,  u2.y, -f.y, 0,
         r.z,  u2.z, -f.z, 0,
        -dot(r,eye), -dot(u2,eye), dot(f,eye), 1
    };

    // Projection matrix
    float t = 1.f / tanf(fovY*0.5f);
    float rng = farZ - nearZ;
    float proj[16] = {
        t/aspect, 0,  0,  0,
        0,  t,  0,  0,
        0,  0, -(farZ+nearZ)/rng, -1,
        0,  0, -2*farZ*nearZ/rng,  0
    };

    // MVP = proj * view (column-major multiply)
    for (int row=0; row<4; row++) {
        for (int col=0; col<4; col++) {
            float s=0;
            for (int k=0; k<4; k++)
                s += proj[row+k*4] * view[k+col*4];
            cam.viewProj[row+col*4] = s;
        }
    }
}

// ════════════════════════════════════════════════════════════
//  TRANSLATE GIZMO
// ════════════════════════════════════════════════════════════
static void DrawTranslateGizmo(ImDrawList* dl, SceneNode& node,
                                 const GizmoCamera& cam) {
    Vec3 origin = {node.translation[0], node.translation[1],
                   node.translation[2]};
    Vec3 axes[3] = {{1,0,0},{0,1,0},{0,0,1}};
    const char* labels[3] = {"X","Y","Z"};
    float gLen = g_gizmo.screenSize / cam.viewSize.x * 6.f;
    ImVec2 mousePos = ImGui::GetIO().MousePos;

    ImVec2 originSS = WorldToScreen(origin, cam);

    for (int i = 0; i < 3; i++) {
        Vec3   tip    = origin + axes[i] * gLen;
        ImVec2 tipSS  = WorldToScreen(tip, cam);

        // Axis direction on screen
        ImVec2 dir = {tipSS.x-originSS.x, tipSS.y-originSS.y};
        float  dl2 = sqrtf(dir.x*dir.x+dir.y*dir.y);
        if (dl2 > 0.1f) { dir.x/=dl2; dir.y/=dl2; }
        g_gizmo.axisDirSS[i] = dir;

        bool hov = false;
        // Hit test: distance from line segment
        ImVec2 p  = mousePos;
        ImVec2 a  = originSS, b = tipSS;
        ImVec2 ab = {b.x-a.x,b.y-a.y};
        float  t  = ((p.x-a.x)*ab.x+(p.y-a.y)*ab.y)
                   /(ab.x*ab.x+ab.y*ab.y+0.001f);
        t = std::clamp(t,0.f,1.f);
        float dx2 = p.x-(a.x+ab.x*t), dy2=p.y-(a.y+ab.y*t);
        if (sqrtf(dx2*dx2+dy2*dy2) < 14.f) {
            hov = true;
            if (g_gizmo.hoveredAxis != i) g_gizmo.hoveredAxis = i;
        }

        bool act = (g_gizmo.activeAxis == i);
        ImU32 c   = AxisColor(i, hov, act);
        float thick = act ? 3.5f : 2.f;

        dl->AddLine(originSS, tipSS, c, thick);

        // Arrowhead
        float aw = 8.f, al = 14.f;
        ImVec2 perp = {-dir.y, dir.x};
        ImVec2 arr[3] = {
            {tipSS.x+dir.x*al*0.5f, tipSS.y+dir.y*al*0.5f},
            {tipSS.x-dir.x*al*0.2f+perp.x*aw*0.5f,
             tipSS.y-dir.y*al*0.2f+perp.y*aw*0.5f},
            {tipSS.x-dir.x*al*0.2f-perp.x*aw*0.5f,
             tipSS.y-dir.y*al*0.2f-perp.y*aw*0.5f}
        };
        dl->AddTriangleFilled(arr[0],arr[1],arr[2],c);

        // Axis label
        dl->AddText({tipSS.x+dir.x*6, tipSS.y+dir.y*6},
            c, labels[i]);

        // Handle drag
        if (hov && ImGui::IsMouseClicked(0) && g_gizmo.activeAxis<0) {
            g_gizmo.activeAxis = i;
            g_gizmo.dragging   = true;
            g_gizmo.dragStart  = mousePos;
            g_gizmo.dragStartVal[0]=node.translation[0];
            g_gizmo.dragStartVal[1]=node.translation[1];
            g_gizmo.dragStartVal[2]=node.translation[2];
        }
    }

    // Centre cube
    dl->AddRectFilled({originSS.x-5,originSS.y-5},
                      {originSS.x+5,originSS.y+5},
                      IM_COL32(220,220,220,200));

    // Apply drag
    if (g_gizmo.dragging && g_gizmo.activeAxis >= 0) {
        if (ImGui::IsMouseDown(0)) {
            ImVec2 d  = {mousePos.x-g_gizmo.dragStart.x,
                         mousePos.y-g_gizmo.dragStart.y};
            int i     = g_gizmo.activeAxis;
            float proj2 = d.x*g_gizmo.axisDirSS[i].x
                        + d.y*g_gizmo.axisDirSS[i].y;
            // World-space delta
            float wdelta = proj2 * gLen / g_gizmo.screenSize;
            node.translation[i] = g_gizmo.dragStartVal[i] + wdelta;
            GJNI().SetTranslation(node.id,
                node.translation[0],
                node.translation[1],
                node.translation[2]);
        } else {
            g_gizmo.dragging   = false;
            g_gizmo.activeAxis = -1;
        }
    }
}

// ════════════════════════════════════════════════════════════
//  ROTATE GIZMO  (3 rings)
// ════════════════════════════════════════════════════════════
static void DrawRotateGizmo(ImDrawList* dl, SceneNode& node,
                              const GizmoCamera& cam) {
    Vec3   origin = {node.translation[0], node.translation[1],
                     node.translation[2]};
    ImVec2 originSS = WorldToScreen(origin, cam);
    float  r       = g_gizmo.screenSize;
    ImVec2 mousePos = ImGui::GetIO().MousePos;

    // Draw 3 rings: X (YZ plane), Y (XZ plane), Z (XY plane)
    const int   SEGS      = 48;
    const float THICK     = 3.f;
    const float HIT_DIST  = 12.f;

    Vec3 axes[3] = {{1,0,0},{0,1,0},{0,0,1}};
    Vec3 tangents1[3] = {{0,1,0},{1,0,0},{1,0,0}};
    Vec3 tangents2[3] = {{0,0,1},{0,0,1},{0,1,0}};

    float gLen = r / cam.viewSize.x * 6.f;

    for (int ax = 0; ax < 3; ax++) {
        bool hov = false;
        std::vector<ImVec2> pts;
        float minDist = 1e9f;

        for (int s = 0; s <= SEGS; s++) {
            float a = (float)s / SEGS * 6.2832f;
            Vec3 wp = origin
                + tangents1[ax] * (cosf(a)*gLen)
                + tangents2[ax] * (sinf(a)*gLen);
            ImVec2 ss = WorldToScreen(wp, cam);
            pts.push_back(ss);

            float dx2=ss.x-mousePos.x, dy2=ss.y-mousePos.y;
            minDist = std::min(minDist, sqrtf(dx2*dx2+dy2*dy2));
        }
        hov = (minDist < HIT_DIST);
        if (hov) g_gizmo.hoveredAxis = ax + 10; // offset to avoid conflict with translate

        bool act = (g_gizmo.activeAxis == ax+10);
        ImU32 c  = AxisColor(ax, hov, act);

        for (int s = 0; s < (int)pts.size()-1; s++)
            dl->AddLine(pts[s], pts[s+1], c,
                        act ? THICK+1.5f : THICK);

        if (hov && ImGui::IsMouseClicked(0) && g_gizmo.activeAxis<0) {
            g_gizmo.activeAxis   = ax+10;
            g_gizmo.dragging     = true;
            g_gizmo.dragStart    = mousePos;
            g_gizmo.dragStartVal[0]=node.rotation[0];
            g_gizmo.dragStartVal[1]=node.rotation[1];
            g_gizmo.dragStartVal[2]=node.rotation[2];
        }
    }

    if (g_gizmo.dragging && g_gizmo.activeAxis >= 10) {
        if (ImGui::IsMouseDown(0)) {
            ImVec2 d = {mousePos.x-g_gizmo.dragStart.x,
                        mousePos.y-g_gizmo.dragStart.y};
            int i    = g_gizmo.activeAxis - 10;
            float delta = (d.x + d.y) * 0.5f; // deg per pixel
            node.rotation[i] = g_gizmo.dragStartVal[i] + delta;
            GJNI().SetRotation(node.id,
                node.rotation[0], node.rotation[1], node.rotation[2]);
        } else {
            g_gizmo.dragging   = false;
            g_gizmo.activeAxis = -1;
        }
    }
}

// ════════════════════════════════════════════════════════════
//  SCALE GIZMO  (arrows + cubes at tips)
// ════════════════════════════════════════════════════════════
static void DrawScaleGizmo(ImDrawList* dl, SceneNode& node,
                             const GizmoCamera& cam) {
    Vec3   origin = {node.translation[0], node.translation[1],
                     node.translation[2]};
    Vec3 axes[3] = {{1,0,0},{0,1,0},{0,0,1}};
    const char* labels[3] = {"X","Y","Z"};
    float gLen = g_gizmo.screenSize / cam.viewSize.x * 6.f;
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    ImVec2 originSS = WorldToScreen(origin, cam);

    for (int i = 0; i < 3; i++) {
        Vec3   tip   = origin + axes[i] * gLen;
        ImVec2 tipSS = WorldToScreen(tip, cam);
        ImVec2 dir   = {tipSS.x-originSS.x, tipSS.y-originSS.y};
        float  dl2   = sqrtf(dir.x*dir.x+dir.y*dir.y);
        if (dl2>0.1f){dir.x/=dl2;dir.y/=dl2;}

        // Hit test (cube area at tip)
        float dx2=tipSS.x-mousePos.x, dy2=tipSS.y-mousePos.y;
        bool hov = (sqrtf(dx2*dx2+dy2*dy2) < 14.f);
        if (hov) g_gizmo.hoveredAxis = i+20;
        bool act = (g_gizmo.activeAxis == i+20);

        ImU32 c = AxisColor(i, hov, act);
        dl->AddLine(originSS, tipSS, c, 2.f);

        // Cube at tip
        float hs = act ? 8.f : 6.f;
        dl->AddRectFilled({tipSS.x-hs,tipSS.y-hs},
                          {tipSS.x+hs,tipSS.y+hs}, c);

        dl->AddText({tipSS.x+10,tipSS.y-6}, c, labels[i]);

        if (hov && ImGui::IsMouseClicked(0) && g_gizmo.activeAxis<0) {
            g_gizmo.activeAxis   = i+20;
            g_gizmo.dragging     = true;
            g_gizmo.dragStart    = mousePos;
            g_gizmo.dragStartVal[0]=node.scale[0];
            g_gizmo.dragStartVal[1]=node.scale[1];
            g_gizmo.dragStartVal[2]=node.scale[2];
        }

        if (g_gizmo.dragging && g_gizmo.activeAxis == i+20) {
            if (ImGui::IsMouseDown(0)) {
                ImVec2 d  = {mousePos.x-g_gizmo.dragStart.x,
                             mousePos.y-g_gizmo.dragStart.y};
                float proj2 = d.x*dir.x + d.y*dir.y;
                float s    = g_gizmo.dragStartVal[i]
                           + proj2 * 0.01f;
                node.scale[i] = std::max(0.001f, s);
                GJNI().SetScale(node.id,
                    node.scale[0], node.scale[1], node.scale[2]);
            } else {
                g_gizmo.dragging   = false;
                g_gizmo.activeAxis = -1;
            }
        }
    }

    // Centre sphere
    dl->AddCircleFilled(originSS, 6.f, IM_COL32(220,220,220,200));
}

// ════════════════════════════════════════════════════════════
//  MAIN: called from viewport each frame
// ════════════════════════════════════════════════════════════
void RenderTransformGizmo(ImDrawList* dl,
                           ImVec2 viewPos, ImVec2 viewSize) {
    auto& e = GEditor();
    if (!e.viewportGizmos) return;
    if (e.playState != PlayState::STOPPED) return;

    SceneNode* node = nullptr;
    for (auto& n : e.sceneNodes)
        if (n.id == e.selectedNodeId) { node = &n; break; }
    if (!node) return;

    // Build camera (placeholder – in production this comes from JME)
    static float camAngleY = 0.3f, camAngleX = 0.4f;
    if (ImGui::IsKeyDown(ImGuiKey_LeftArrow)) camAngleY -= 0.02f;
    if (ImGui::IsKeyDown(ImGuiKey_RightArrow)) camAngleY += 0.02f;
    if (ImGui::IsKeyDown(ImGuiKey_UpArrow))   camAngleX -= 0.02f;
    if (ImGui::IsKeyDown(ImGuiKey_DownArrow)) camAngleX += 0.02f;

    float dist = 8.f;
    Vec3 eye = {
        dist*cosf(camAngleX)*sinf(camAngleY),
        dist*sinf(camAngleX),
        dist*cosf(camAngleX)*cosf(camAngleY)
    };
    eye.x += node->translation[0];
    eye.y += node->translation[1];
    eye.z += node->translation[2];

    GizmoCamera cam;
    cam.viewPos  = viewPos;
    cam.viewSize = viewSize;
    BuildViewProj(cam, eye,
        {node->translation[0], node->translation[1], node->translation[2]},
        {0,1,0}, 1.0f,
        viewSize.x/(viewSize.y>0?viewSize.y:1),
        0.1f, 1000.f);

    g_gizmo.screenSize = std::min(viewSize.x, viewSize.y) * 0.12f;
    g_gizmo.hoveredAxis = -1;

    switch(e.gizmoMode) {
    case 0: DrawTranslateGizmo(dl, *node, cam); break;
    case 1: DrawRotateGizmo(dl,    *node, cam); break;
    case 2: DrawScaleGizmo(dl,     *node, cam); break;
    }

    // XYZ orientation cube (top-right corner)
    ImVec2 cubeSS = {viewPos.x+viewSize.x-50, viewPos.y+50};
    GizmoCamera smallCam = cam;
    smallCam.viewPos  = {cubeSS.x-30, cubeSS.y-30};
    smallCam.viewSize = {60,60};
    BuildViewProj(smallCam, eye, {0,0,0}, {0,1,0},
        1.0f, 1.f, 0.1f, 100.f);

    Vec3 org = {0,0,0};
    const char* lbs[3]={"X","Y","Z"};
    Vec3 axDirs[3]={{0.8f,0,0},{0,0.8f,0},{0,0,0.8f}};
    for (int i=0;i<3;i++){
        ImVec2 o2=WorldToScreen(org,smallCam);
        ImVec2 t2=WorldToScreen(axDirs[i],smallCam);
        dl->AddLine(o2,t2,AxisColor(i),2.f);
        dl->AddText({t2.x,t2.y-8},AxisColor(i),lbs[i]);
    }
}
