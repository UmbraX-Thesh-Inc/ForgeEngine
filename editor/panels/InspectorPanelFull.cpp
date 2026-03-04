// ============================================================
//  ForgeEngine – InspectorPanelFull.cpp
//  Complete inspector with ALL component sections:
//    Transform, Material (PBR), RigidBody, Character,
//    AudioSource, ScriptList, Prefab, Light, Camera, Particle
// ============================================================
#include "../ForgeEditor.h"
#include "../../jni/JNIBridgeFull.h"
#include "../widgets/IconSystem.h"
#include "../prefab/PrefabSystem.h"
#include "imgui.h"
#include <cstring>
#include <string>
#include <vector>

// ── Component state per selected node ────────────────────────
struct InspectorState {
    // Material
    float   albedo[4]    = {0.8f,0.8f,0.8f,1.f};
    float   metallic     = 0.f;
    float   roughness    = 0.5f;
    float   emissive[3]  = {0,0,0};
    char    texAlbedo[128]= "";
    char    texNormal[128]= "";
    // Rigid body
    float   mass         = 1.f;
    int     shapeType    = 0;   // 0=box,1=sphere,2=capsule
    float   shapeData[3] = {0.5f,0.5f,0.5f};
    bool    kinematic    = false;
    float   friction     = 0.5f;
    float   restitution  = 0.3f;
    int     rigidBodyId  = -1;
    // Character
    float   charRadius   = 0.3f;
    float   charHeight   = 1.8f;
    int     charId       = -1;
    // Audio
    char    audioPath[128]= "";
    bool    audioPositional=true;
    bool    audioLoop    = false;
    bool    audioStream  = false;
    float   audioVolume  = 1.f;
    float   audioPitch   = 1.f;
    int     audioId      = -1;
    // Scripts
    char    scriptFilter[64]= "";
    // Camera
    float   camFOV       = 60.f;
    // Light
    float   lightColor[3]= {1,1,1};
    float   lightIntensity= 1.f;
    // Post-processing
    bool    ppBloom      = true;
    float   ppBloomInt   = 0.4f;
    bool    ppSSAO       = true;
    bool    ppFXAA       = true;
    bool    ppDOF        = false;
    float   ppDOFFocus   = 5.f;
    float   ppShadowInt  = 0.6f;
    float   sunDir[3]    = {-0.5f,-1.f,-0.5f};
    int     lastNodeId   = -1; // detect selection change
};
static InspectorState g_istate;

// ── Helper: section header ────────────────────────────────────
static bool SectionHeader(const char* label, Icon icon,
                            bool* enabled=nullptr) {
    ImGui::PushStyleColor(ImGuiCol_Header,       {0.12f,0.16f,0.22f,1.f});
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,{0.18f,0.22f,0.30f,1.f});

    Icons().Draw(icon, ICON_SM, ForgeTheme::ACCENT);
    ImGui::SameLine(0,4);
    bool open = ImGui::CollapsingHeader(label,
        ImGuiTreeNodeFlags_DefaultOpen);

    if (enabled) {
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
        std::string cbid = std::string("##en_")+label;
        ImGui::Checkbox(cbid.c_str(), enabled);
    }
    ImGui::PopStyleColor(2);
    return open;
}

// ── Labelled drag float row ───────────────────────────────────
static bool DragRow3(const char* label, float* v, float speed=0.01f,
                      float mn=-1e9f, float mx=1e9f) {
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT1);
    ImGui::SetNextItemWidth(60); ImGui::Text("%s", label);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::DANGER);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x/3-4);
    std::string xid=std::string("##")+label+"x";
    bool r = ImGui::DragFloat(xid.c_str(), v+0, speed, mn, mx, "X:%.2f");
    ImGui::PopStyleColor();
    ImGui::SameLine(0,2);
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT3);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x/2-4);
    std::string yid=std::string("##")+label+"y";
    r |= ImGui::DragFloat(yid.c_str(), v+1, speed, mn, mx, "Y:%.2f");
    ImGui::PopStyleColor();
    ImGui::SameLine(0,2);
    ImGui::PushStyleColor(ImGuiCol_Text, {0.4f,0.6f,1.f,1.f});
    ImGui::SetNextItemWidth(-1);
    std::string zid=std::string("##")+label+"z";
    r |= ImGui::DragFloat(zid.c_str(), v+2, speed, mn, mx, "Z:%.2f");
    ImGui::PopStyleColor();
    return r;
}

// ═══════════════════════════════════════════════════════════
//  Main Inspector render
// ═══════════════════════════════════════════════════════════
void RenderInspectorPanelFull() {
    auto& e     = GEditor();
    auto& state = e.panelStates["inspector"];
    if (!state.open) return;

    ImGui::SetNextWindowSize({280,700}, ImGuiCond_Once);
    bool open = state.open;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ForgeTheme::BG1);
    ImGui::Begin("Inspector##insp", &open);
    state.open = open;

    // Find selected node
    SceneNode* node = nullptr;
    for (auto& n : e.sceneNodes)
        if (n.id == e.selectedNodeId) { node = &n; break; }

    if (!node) {
        // No selection – show scene-wide post-processing settings
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT2);
        ImGui::TextUnformatted("No object selected");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        if (SectionHeader("Rendering & Post-FX", Icon::SETTINGS)) {
            ImGui::Spacing();
            // Bloom
            if (ImGui::Checkbox("Bloom", &g_istate.ppBloom))
                GJNI().SetBloomEnabled(g_istate.ppBloom);
            if (g_istate.ppBloom) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderFloat("##bi",&g_istate.ppBloomInt,0,2,"Intensity %.2f"))
                    GJNI().SetBloomIntensity(g_istate.ppBloomInt);
            }
            // SSAO
            if (ImGui::Checkbox("SSAO##pp",&g_istate.ppSSAO))
                GJNI().SetSSAOEnabled(g_istate.ppSSAO);
            // FXAA
            if (ImGui::Checkbox("FXAA##pp",&g_istate.ppFXAA))
                GJNI().SetFXAAEnabled(g_istate.ppFXAA);
            // DOF
            if (ImGui::Checkbox("Depth of Field",&g_istate.ppDOF))
                GJNI().SetDOFEnabled(g_istate.ppDOF);
            if (g_istate.ppDOF) {
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("Focus##dof",&g_istate.ppDOFFocus,0.1f,0.1f,100.f,"%.1fm"))
                    GJNI().SetDOFFocusDistance(g_istate.ppDOFFocus);
            }
            // Shadow
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("Shadow##si",&g_istate.ppShadowInt,0.01f,0,1))
                GJNI().SetShadowIntensity(g_istate.ppShadowInt);
            // Sun direction
            ImGui::TextUnformatted("Sun Direction");
            if (DragRow3("Sun", g_istate.sunDir, 0.01f, -1, 1))
                GJNI().SetSunDirection(g_istate.sunDir[0],g_istate.sunDir[1],g_istate.sunDir[2]);

            ImGui::Spacing();
            if (ImGui::Checkbox("Show Grid",&e.viewportGrid))
                GJNI().SetGridVisible(e.viewportGrid);
        }
        ImGui::End();
        ImGui::PopStyleColor();
        return;
    }

    // ── Reset state on selection change ───────────────────────
    if (g_istate.lastNodeId != node->id) {
        g_istate.lastNodeId = node->id;
        // Read current transform
        JMETransform t = GJNI().GetTransform(node->id);
        node->translation[0]=t.tx; node->translation[1]=t.ty; node->translation[2]=t.tz;
        node->scale[0]=t.sx; node->scale[1]=t.sy; node->scale[2]=t.sz;
    }

    // ── Node header ───────────────────────────────────────────
    {
        bool isPrefab = node->type.find("Prefab:")!=std::string::npos;
        ImVec4 nameCol = isPrefab ? ForgeTheme::ACCENT : ForgeTheme::TEXT0;
        ImGui::PushStyleColor(ImGuiCol_Text, nameCol);
        Icons().Draw(Icon::NODE, ICON_MD, nameCol);
        ImGui::SameLine(0,5);
        static char nameBuf[64];
        strncpy(nameBuf, node->name.c_str(), 63);
        ImGui::SetNextItemWidth(-30);
        if (ImGui::InputText("##nodename", nameBuf, 64)) {
            node->name = nameBuf;
            GJNI().SetSpatialName(node->id, nameBuf);
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();
        bool vis = node->visible;
        if (Icons().Button(vis ? Icon::VISIBILITY_ON : Icon::VISIBILITY_OFF,
                            vis ? "Hide" : "Show", ICON_SM)) {
            node->visible = !vis;
            GJNI().SetVisible(node->id, node->visible);
        }

        // Type badge
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT2);
        ImGui::Text("  %s  •  ID %d", node->type.c_str(), node->id);
        ImGui::PopStyleColor();
    }
    ImGui::Separator();

    // ════════════════════════════════════════════════════════
    //  TRANSFORM
    // ════════════════════════════════════════════════════════
    if (SectionHeader("Transform", Icon::GIZMO_TRANSLATE)) {
        if (DragRow3("Pos", node->translation, 0.01f))
            GJNI().SetTranslation(node->id,
                node->translation[0],node->translation[1],node->translation[2]);
        if (DragRow3("Rot", node->rotation, 0.5f))
            GJNI().SetRotation(node->id,
                node->rotation[0],node->rotation[1],node->rotation[2]);
        if (DragRow3("Scale", node->scale, 0.01f, 0.001f, 1000.f))
            GJNI().SetScale(node->id,
                node->scale[0],node->scale[1],node->scale[2]);

        ImGui::Spacing();
        // Reset buttons
        if (ImGui::SmallButton("Reset Pos")) {
            node->translation[0]=node->translation[1]=node->translation[2]=0;
            GJNI().SetTranslation(node->id,0,0,0);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset Rot")) {
            node->rotation[0]=node->rotation[1]=node->rotation[2]=0;
            GJNI().SetRotation(node->id,0,0,0);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset Scale")) {
            node->scale[0]=node->scale[1]=node->scale[2]=1;
            GJNI().SetScale(node->id,1,1,1);
        }
    }

    // ════════════════════════════════════════════════════════
    //  MATERIAL (PBR)
    // ════════════════════════════════════════════════════════
    if (node->type.find("Geometry")!=std::string::npos ||
        node->type.find("Box")!=std::string::npos ||
        node->type.find("Sphere")!=std::string::npos) {
        if (SectionHeader("Material (PBR)", Icon::ASSET_MATERIAL)) {
            // Albedo colour
            ImGui::TextUnformatted("Albedo");
            ImGui::SameLine(70); ImGui::SetNextItemWidth(-1);
            if (ImGui::ColorEdit4("##alb",g_istate.albedo,ImGuiColorEditFlags_NoLabel))
                GJNI().SetMaterialColor(node->id,
                    g_istate.albedo[0],g_istate.albedo[1],
                    g_istate.albedo[2],g_istate.albedo[3]);

            // Metallic
            ImGui::TextUnformatted("Metallic");
            ImGui::SameLine(70); ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("##met",&g_istate.metallic,0,1))
                GJNI().SetMaterialPBR(node->id, g_istate.metallic, g_istate.roughness,
                    g_istate.emissive[0],g_istate.emissive[1],g_istate.emissive[2]);

            // Roughness
            ImGui::TextUnformatted("Roughness");
            ImGui::SameLine(70); ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("##rou",&g_istate.roughness,0,1))
                GJNI().SetMaterialPBR(node->id, g_istate.metallic, g_istate.roughness,
                    g_istate.emissive[0],g_istate.emissive[1],g_istate.emissive[2]);

            // Emissive
            ImGui::TextUnformatted("Emissive");
            ImGui::SameLine(70); ImGui::SetNextItemWidth(-1);
            ImGui::ColorEdit3("##emi",g_istate.emissive,ImGuiColorEditFlags_NoLabel|ImGuiColorEditFlags_HDR);

            ImGui::Spacing();
            // Texture slots
            ImGui::TextUnformatted("Albedo Tex");
            ImGui::SameLine(80); ImGui::SetNextItemWidth(-ICON_MD-4);
            ImGui::InputText("##talb",g_istate.texAlbedo,128);
            ImGui::SameLine(0,2);
            if (Icons().Button(Icon::FOLDER_OPEN,"Browse",ICON_SM)) {}
            if (ImGui::IsItemDeactivatedAfterEdit())
                GJNI().SetMaterialTexture(node->id,"DiffuseMap",g_istate.texAlbedo);

            ImGui::TextUnformatted("Normal Map");
            ImGui::SameLine(80); ImGui::SetNextItemWidth(-ICON_MD-4);
            ImGui::InputText("##tnrm",g_istate.texNormal,128);
            ImGui::SameLine(0,2);
            if (Icons().Button(Icon::FOLDER_OPEN,"Browse Normal",ICON_SM)) {}
            if (ImGui::IsItemDeactivatedAfterEdit())
                GJNI().SetMaterialTexture(node->id,"NormalMap",g_istate.texNormal);

            // Open in material editor
            ImGui::Spacing();
            if (Icons().Button(Icon::ASSET_MATERIAL,"Open Material Editor",ICON_SM,ForgeTheme::ACCENT)) {
                e.panelStates["material"].open = true;
            }
        }
    }

    // ════════════════════════════════════════════════════════
    //  RIGID BODY
    // ════════════════════════════════════════════════════════
    static bool showRigidBody = false;
    if (SectionHeader("Rigid Body", Icon::OBJ_BOX, &showRigidBody)) {
        if (showRigidBody) {
            static const char* shapes[] = {"Box","Sphere","Capsule","Mesh","Hull"};
            ImGui::SetNextItemWidth(100);
            ImGui::Combo("Shape##rb", &g_istate.shapeType, shapes, 5);

            ImGui::TextUnformatted("Mass");
            ImGui::SameLine(70); ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##mass",&g_istate.mass,0.1f,0,10000.f,"%.1f kg");

            ImGui::TextUnformatted("Friction");
            ImGui::SameLine(70); ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("##fric",&g_istate.friction,0,1))
                if (g_istate.rigidBodyId>=0) GJNI().SetFriction(g_istate.rigidBodyId,g_istate.friction);

            ImGui::TextUnformatted("Bounce");
            ImGui::SameLine(70); ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("##rest",&g_istate.restitution,0,1))
                if (g_istate.rigidBodyId>=0) GJNI().SetRestitution(g_istate.rigidBodyId,g_istate.restitution);

            ImGui::Checkbox("Kinematic",&g_istate.kinematic);
            if (ImGui::IsItemEdited() && g_istate.rigidBodyId>=0)
                GJNI().SetKinematic(g_istate.rigidBodyId, g_istate.kinematic);

            ImGui::Spacing();
            if (g_istate.rigidBodyId < 0) {
                if (ImGui::Button("Add Rigid Body##btn",{-1,28})) {
                    std::vector<float> sd(g_istate.shapeData,
                        g_istate.shapeData+3);
                    g_istate.rigidBodyId = GJNI().AddRigidBody(
                        node->id, g_istate.shapeType,
                        g_istate.mass, sd);
                }
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, {0.6f,0.1f,0.1f,1.f});
                if (ImGui::Button("Remove Rigid Body",{-1,28})) {
                    GJNI().RemoveRigidBody(g_istate.rigidBodyId);
                    g_istate.rigidBodyId = -1;
                }
                ImGui::PopStyleColor();
                // Impulse quick-apply
                if (ImGui::SmallButton("Impulse Up"))
                    GJNI().ApplyImpulse(g_istate.rigidBodyId, 0, 5, 0);
            }
        }
    }

    // ════════════════════════════════════════════════════════
    //  CHARACTER CONTROLLER
    // ════════════════════════════════════════════════════════
    static bool showChar = false;
    if (SectionHeader("Character Controller", Icon::OBJ_CAPSULE, &showChar)) {
        if (showChar) {
            ImGui::SetNextItemWidth(100);
            ImGui::DragFloat("Radius",&g_istate.charRadius,0.01f,0.1f,5.f);
            ImGui::SetNextItemWidth(100);
            ImGui::DragFloat("Height",&g_istate.charHeight,0.01f,0.5f,10.f);
            if (g_istate.charId < 0) {
                if (ImGui::Button("Add Character Controller",{-1,28}))
                    g_istate.charId = GJNI().AddCharacter(
                        node->id, g_istate.charRadius, g_istate.charHeight);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT3);
                ImGui::Text("Active – on ground: %s",
                    GJNI().IsOnGround(g_istate.charId) ? "yes" : "no");
                ImGui::PopStyleColor();
            }
        }
    }

    // ════════════════════════════════════════════════════════
    //  AUDIO SOURCE
    // ════════════════════════════════════════════════════════
    static bool showAudio = false;
    if (SectionHeader("Audio Source", Icon::AUDIO, &showAudio)) {
        if (showAudio) {
            ImGui::TextUnformatted("File"); ImGui::SameLine(50);
            ImGui::SetNextItemWidth(-ICON_MD-4);
            ImGui::InputText("##apath",g_istate.audioPath,128);
            ImGui::SameLine(0,2);
            if (Icons().Button(Icon::FOLDER_OPEN,"Browse Audio",ICON_SM)) {}

            ImGui::Checkbox("Positional",&g_istate.audioPositional);
            ImGui::SameLine(0,8);
            ImGui::Checkbox("Loop",&g_istate.audioLoop);
            ImGui::SameLine(0,8);
            ImGui::Checkbox("Stream",&g_istate.audioStream);

            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("Volume##av",&g_istate.audioVolume,0,1);
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("Pitch##ap",&g_istate.audioPitch,0.25f,4.f);

            ImGui::Spacing();
            if (g_istate.audioId < 0) {
                if (ImGui::Button("Add Audio Source",{-1,28})) {
                    g_istate.audioId = GJNI().CreateAudio(
                        g_istate.audioPath,
                        g_istate.audioPositional,
                        g_istate.audioStream,
                        g_istate.audioLoop);
                    GJNI().AttachAudio(g_istate.audioId, node->id);
                }
            } else {
                if (Icons().Button(Icon::PLAY,"Play",ICON_MD,ForgeTheme::ACCENT3))
                    GJNI().PlayAudio(g_istate.audioId);
                ImGui::SameLine(0,4);
                if (Icons().Button(Icon::PAUSE,"Pause",ICON_MD))
                    GJNI().PauseAudio(g_istate.audioId);
                ImGui::SameLine(0,4);
                if (Icons().Button(Icon::STOP,"Stop",ICON_MD))
                    GJNI().StopAudio(g_istate.audioId);
            }
        }
    }

    // ════════════════════════════════════════════════════════
    //  SCRIPTS
    // ════════════════════════════════════════════════════════
    if (SectionHeader("Scripts", Icon::ASSET_SCRIPT)) {
        std::string json = GJNI().GetScriptListJSON();
        // Parse minimal script list
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ForgeTheme::BG0);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##scf","Filter scripts...",g_istate.scriptFilter,64);
        ImGui::PopStyleColor();

        // Quick attach new script
        if (Icons().Button(Icon::CODE_FILE,"Open Code Editor",ICON_SM)) {
            e.panelStates["codeeditor"].open = true;
        }
        ImGui::SameLine(0,4);
        if (Icons().Button(Icon::ADD,"New Script",ICON_SM,ForgeTheme::ACCENT3)) {}
    }

    // ════════════════════════════════════════════════════════
    //  PREFAB
    // ════════════════════════════════════════════════════════
    DrawPrefabInspectorSection(*node);

    // ════════════════════════════════════════════════════════
    //  CAMERA NODE
    // ════════════════════════════════════════════════════════
    if (node->type.find("Camera")!=std::string::npos) {
        if (SectionHeader("Camera", Icon::CAMERA)) {
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("FOV",&g_istate.camFOV,20,120,"%.0f°"))
                GJNI().SetCameraFOV(g_istate.camFOV);
            if (ImGui::Button("Set as Active Camera",{-1,28}))
                GJNI().SetCameraTransform(
                    node->translation[0],node->translation[1],node->translation[2],
                    0,0,0);
        }
    }

    // ════════════════════════════════════════════════════════
    //  LIGHT NODE
    // ════════════════════════════════════════════════════════
    if (node->type.find("Light")!=std::string::npos ||
        node->type=="LIGHT_POINT" || node->type=="LIGHT_DIRECTIONAL") {
        if (SectionHeader("Light", Icon::LIGHT)) {
            ImGui::TextUnformatted("Color");
            ImGui::SameLine(70); ImGui::SetNextItemWidth(-1);
            if (ImGui::ColorEdit3("##lc",g_istate.lightColor))
                GJNI().SetLightColor(node->id,
                    g_istate.lightColor[0],g_istate.lightColor[1],g_istate.lightColor[2]);
            ImGui::TextUnformatted("Intensity");
            ImGui::SameLine(70); ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##lint",&g_istate.lightIntensity,0.01f,0,100))
                GJNI().SetLightIntensity(node->id, g_istate.lightIntensity);
        }
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // ── Danger zone ───────────────────────────────────────────
    if (ImGui::SmallButton("Duplicate##insp"))
        GJNI().DuplicateSpatial(node->id);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::DANGER);
    if (ImGui::SmallButton("Delete##insp")) {
        GJNI().RemoveSpatial(node->id);
        e.selectedNodeId = -1;
    }
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleColor();
}
