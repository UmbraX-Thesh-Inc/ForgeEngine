// ============================================================
//  ForgeEngine – NetworkPanel.cpp
//  Editor panel: host/join servers, view peers, broadcast, chat.
// ============================================================
#include "../ForgeEditor.h"
#include "../../jni/JNIBridgeFull.h"
#include "../widgets/IconSystem.h"
#include "imgui.h"
#include <string>
#include <vector>

struct NetPanelState {
    char   serverHost[64]  = "127.0.0.1";
    int    serverPort       = 7777;
    int    maxPlayers       = 8;
    bool   isHosting        = false;
    bool   isConnected      = false;
    char   chatBuf[256]     = "";
    std::vector<std::string> chatLog;
};
static NetPanelState g_net;

void RenderNetworkPanel() {
    auto& e     = GEditor();
    auto& state = e.panelStates["network"];
    if (!state.open) return;

    ImGui::SetNextWindowSize({320,440}, ImGuiCond_Once);
    bool open = state.open;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ForgeTheme::BG1);
    ImGui::Begin("Network##netwin", &open);
    state.open = open;

    // Header
    ImVec4 statusCol = g_net.isHosting   ? ForgeTheme::ACCENT3
                     : g_net.isConnected ? ForgeTheme::ACCENT
                     : ForgeTheme::TEXT2;
    Icons().Draw(Icon::NODE, ICON_MD, statusCol);
    ImGui::SameLine(0,5);
    ImGui::PushStyleColor(ImGuiCol_Text, statusCol);
    ImGui::TextUnformatted(g_net.isHosting   ? "Hosting Server"
                         : g_net.isConnected ? "Connected"
                         : "Offline");
    ImGui::PopStyleColor();
    ImGui::Separator();

    if (!g_net.isHosting && !g_net.isConnected) {
        // ── Host ─────────────────────────────────────────────
        ForgeUI::SectionHeader("Host Server");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputInt("Port##hp", &g_net.serverPort);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputInt("Max Players", &g_net.maxPlayers);
        ImGui::PushStyleColor(ImGuiCol_Button,
            {ForgeTheme::ACCENT.x*0.2f,ForgeTheme::ACCENT.y*0.2f,ForgeTheme::ACCENT.z*0.2f,1});
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT);
        if (ImGui::Button("Start Server",{-1,30})) {
            if (GJNI().HostServer(g_net.serverPort, g_net.maxPlayers)) {
                g_net.isHosting = true;
                g_net.chatLog.push_back("[System] Server started on port " +
                    std::to_string(g_net.serverPort));
            }
        }
        ImGui::PopStyleColor(2);

        ImGui::Spacing();

        // ── Join ─────────────────────────────────────────────
        ForgeUI::SectionHeader("Join Server");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("Host##jh", g_net.serverHost, 64);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputInt("Port##jp", &g_net.serverPort);
        ImGui::PushStyleColor(ImGuiCol_Button,
            {ForgeTheme::ACCENT3.x*0.2f,ForgeTheme::ACCENT3.y*0.2f,ForgeTheme::ACCENT3.z*0.2f,1});
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::ACCENT3);
        if (ImGui::Button("Connect",{-1,30})) {
            if (GJNI().ConnectServer(g_net.serverHost, g_net.serverPort)) {
                g_net.isConnected = true;
                g_net.chatLog.push_back("[System] Connected to " +
                    std::string(g_net.serverHost));
            } else {
                g_net.chatLog.push_back("[System] Connection failed");
            }
        }
        ImGui::PopStyleColor(2);
    } else {
        // ── Online panel ─────────────────────────────────────
        std::string netJson = GJNI().GetNetworkStatusJSON();
        // Parse peer count from JSON minimally
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT1);
        ImGui::TextUnformatted(netJson.c_str());
        ImGui::PopStyleColor();

        ImGui::Spacing();
        if (ImGui::Button("Ping All",{100,26})) GJNI().PingNetwork();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, {0.5f,0.1f,0.1f,1.f});
        ImGui::PushStyleColor(ImGuiCol_Text,   ForgeTheme::DANGER);
        if (ImGui::Button("Disconnect",{-1,26})) {
            GJNI().DisconnectNetwork();
            g_net.isHosting   = false;
            g_net.isConnected = false;
            g_net.chatLog.push_back("[System] Disconnected");
        }
        ImGui::PopStyleColor(2);
    }

    ImGui::Spacing();
    ImGui::Separator();

    // ── Chat ──────────────────────────────────────────────────
    ForgeUI::SectionHeader("Chat");
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ForgeTheme::BG0);
    ImGui::BeginChild("##chatlog",{0,-36},false);
    for (auto& line : g_net.chatLog) {
        ImGui::PushStyleColor(ImGuiCol_Text, ForgeTheme::TEXT1);
        ImGui::TextUnformatted(line.c_str());
        ImGui::PopStyleColor();
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.f);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SetNextItemWidth(-ICON_MD-4);
    bool send = ImGui::InputText("##chatinput", g_net.chatBuf, 256,
        ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine(0,2);
    send |= Icons().Button(Icon::PLAY,"Send",ICON_MD,ForgeTheme::ACCENT);
    if (send && g_net.chatBuf[0]) {
        std::string msg(g_net.chatBuf);
        GJNI().SendChat(msg);
        g_net.chatLog.push_back("[Me] " + msg);
        memset(g_net.chatBuf, 0, sizeof(g_net.chatBuf));
    }

    ImGui::End();
    ImGui::PopStyleColor();
}
