#include "Editor/UI/EditorMainPanel.h"

#include "ImGui/imgui.h"

bool FEditorMainPanel::DrawViewportTextButton(
    const char* Id,
    const char* Label,
    bool bPairFirst,
    bool bPairSecond)
{
    const ImVec2 TextSize = ImGui::CalcTextSize(Label);
    const ImVec2 Padding = ImGui::GetStyle().FramePadding;
    const ImVec2 ButtonSize(TextSize.x + Padding.x * 2.0f, ImGui::GetFrameHeight());
    const bool bPressed = ImGui::InvisibleButton(Id, ButtonSize);
    const ImVec2 Min = ImGui::GetItemRectMin();
    const ImVec2 Max = ImGui::GetItemRectMax();
    const bool bHovered = ImGui::IsItemHovered();
    const bool bHeld = ImGui::IsItemActive();
    const ImU32 BgColor = ImGui::GetColorU32(bHeld ? ImGuiCol_ButtonActive : (bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
    ImDrawFlags RoundFlags = ImDrawFlags_RoundCornersAll;
    if (bPairFirst)
    {
        RoundFlags = ImDrawFlags_RoundCornersLeft;
    }
    if (bPairSecond)
    {
        RoundFlags = ImDrawFlags_RoundCornersRight;
    }

    ImGui::GetWindowDrawList()->AddRectFilled(Min, Max, BgColor, ImGui::GetStyle().FrameRounding, RoundFlags);
    const ImVec2 TextPos(Min.x + (ButtonSize.x - TextSize.x) * 0.5f, Min.y + (ButtonSize.y - TextSize.y) * 0.5f);
    ImGui::GetWindowDrawList()->AddText(TextPos, ImGui::GetColorU32(ImGuiCol_Text), Label);
    ImGui::GetWindowDrawList()->AddText(ImVec2(TextPos.x + 0.8f, TextPos.y), ImGui::GetColorU32(ImGuiCol_Text), Label);
    return bPressed;
}

bool FEditorMainPanel::DrawViewportIconButton(
    const char* Id,
    EEditorMainPanelViewportToolIcon Icon,
    const char* FallbackLabel,
    const char* Tooltip,
    bool bSelected,
    bool bEnabled,
    bool bPairFirst,
    bool bPairSecond)
{
    if (!bEnabled)
    {
        ImGui::BeginDisabled();
    }

    if (bSelected)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.33f, 0.46f, 0.63f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.37f, 0.52f, 0.70f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.27f, 0.39f, 0.54f, 1.0f));
    }

    ID3D11ShaderResourceView* SRV = IconResources.ToolIcons[static_cast<int32>(Icon)];
    bool bPressed = false;
    if (!SRV)
    {
        bPressed = DrawViewportTextButton(Id, FallbackLabel, bPairFirst, bPairSecond);
    }
    else
    {
        constexpr ImVec2 IconSize(16.0f, 16.0f);
        const ImVec2 Padding = ImGui::GetStyle().FramePadding;
        const ImVec2 ButtonSize(IconSize.x + Padding.x * 2.0f, ImGui::GetFrameHeight());
        bPressed = ImGui::InvisibleButton(Id, ButtonSize);
        const ImVec2 Min = ImGui::GetItemRectMin();
        const ImVec2 Max = ImGui::GetItemRectMax();
        const bool bHovered = ImGui::IsItemHovered();
        const bool bHeld = ImGui::IsItemActive();
        const ImU32 BgColor = ImGui::GetColorU32(bHeld ? ImGuiCol_ButtonActive : (bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
        ImDrawFlags RoundFlags = ImDrawFlags_RoundCornersAll;
        if (bPairFirst)
        {
            RoundFlags = ImDrawFlags_RoundCornersLeft;
        }
        if (bPairSecond)
        {
            RoundFlags = ImDrawFlags_RoundCornersRight;
        }
        ImGui::GetWindowDrawList()->AddRectFilled(Min, Max, BgColor, ImGui::GetStyle().FrameRounding, RoundFlags);
        ImGui::GetWindowDrawList()->AddImage(
            reinterpret_cast<ImTextureID>(SRV),
            ImVec2(Min.x + Padding.x, Min.y + (ButtonSize.y - IconSize.y) * 0.5f),
            ImVec2(Min.x + Padding.x + IconSize.x, Min.y + (ButtonSize.y + IconSize.y) * 0.5f),
            ImVec2(0.0f, 0.0f),
            ImVec2(1.0f, 1.0f),
            ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, bEnabled ? 1.0f : 0.45f)));
    }

    if (bSelected)
    {
        ImGui::PopStyleColor(3);
    }
    if (!bEnabled)
    {
        ImGui::EndDisabled();
    }

    if (Tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::SetTooltip("%s", Tooltip);
    }
    return bEnabled && bPressed;
}
