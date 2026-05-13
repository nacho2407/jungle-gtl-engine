#include "Editor/UI/EditorMainPanel.h"

#include "ImGui/imgui.h"

void FEditorMainPanel::RenderDockSpace()
{
    const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
    if (!MainViewport)
    {
        return;
    }

    constexpr float EditorTabStripHeight = 30.0f;
    constexpr float EditorToolbarHeight = 40.0f;
    constexpr float FooterHeight = 32.0f;
    const float TopChromeHeight = EditorTabStripHeight + EditorToolbarHeight;
    const ImVec2 DockPos(MainViewport->WorkPos.x, MainViewport->WorkPos.y + TopChromeHeight);
    const ImVec2 DockSize(
        MainViewport->WorkSize.x,
        (MainViewport->WorkSize.y > (FooterHeight + TopChromeHeight))
            ? (MainViewport->WorkSize.y - FooterHeight - TopChromeHeight)
            : 0.0f);

    ImGui::SetNextWindowPos(DockPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(DockSize, ImGuiCond_Always);
    ImGui::SetNextWindowViewport(MainViewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    constexpr ImGuiWindowFlags DockHostFlags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground;

    ImGui::Begin("##MainDockHost", nullptr, DockHostFlags);
    ImGui::PopStyleVar(3);

    const ImGuiID DockSpaceId = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(DockSpaceId, ImVec2(0.0f, 0.0f));
    ImGui::End();
}
