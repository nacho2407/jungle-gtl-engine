#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Viewport/ViewportLayout.h"

#include "ImGui/imgui.h"

#include <cstdio>

namespace
{
constexpr int32 DefaultPIEUILayoutWidth = 1920;
constexpr int32 DefaultPIEUILayoutHeight = 1080;

const char* GetMainToolbarViewModeName(EViewMode Mode)
{
    switch (Mode)
    {
    case EViewMode::Lit_Gouraud:
        return "Lit (Gouraud)";
    case EViewMode::Lit_Lambert:
        return "Lit (Lambert)";
    case EViewMode::Lit_BlinnPhong:
        return "Lit (Blinn-Phong)";
    case EViewMode::Unlit:
        return "Unlit";
    case EViewMode::Heatmap:
        return "Heatmap";
    case EViewMode::Wireframe:
        return "Wireframe";
    case EViewMode::Depth:
        return "Depth";
    case EViewMode::Normal:
        return "Normal";
    case EViewMode::IdBuffer:
        return "ID Buffer";
    default:
        return "Lit";
    }
}

} // namespace

void FEditorMainPanel::RenderEditorToolbar()
{
    if (!EditorEngine)
    {
        return;
    }

    ImGuiViewport* MainViewport = ImGui::GetMainViewport();
    if (!MainViewport)
    {
        return;
    }

    constexpr float ToolbarHeight = 40.0f;
    constexpr float ButtonSize = 30.0f;
    const ImVec2 ToolbarPos(MainViewport->WorkPos.x, MainViewport->WorkPos.y);
    const ImVec2 ToolbarSize(MainViewport->WorkSize.x, ToolbarHeight);

    ImGui::SetNextWindowPos(ToolbarPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ToolbarSize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 5.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.11f, 0.14f, 0.98f));

    constexpr ImGuiWindowFlags Flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoDocking;

    if (ImGui::Begin("##EditorToolbarWeek06", nullptr, Flags))
    {
        FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
        const int32 FocusedIndex = Layout.GetLastFocusedViewportIndex();
        FEditorViewportClient* Client = Layout.GetViewportClient(FocusedIndex);
        const FVector ToolbarSpawnLocation = FVector(0.0f, 0.0f, 0.0f);

        const bool bEditing = EditorEngine->GetEditorState() == EViewportPlayState::Editing;
        const bool bCanPlaceActor = Client && Client->AllowsEditorWorldControl();
        const bool bCanSave = bEditing;
        ImDrawList* DrawList = ImGui::GetWindowDrawList();

        ImGui::SetCursorPos(ImVec2(10.0f, 5.0f));
        if (!bCanSave)
        {
            ImGui::BeginDisabled();
        }
        const bool bSaveClicked = ImGui::InvisibleButton("##ToolbarSaveScene", ImVec2(ButtonSize, ButtonSize));
        const ImVec2 SaveMin = ImGui::GetItemRectMin();
        const ImVec2 SaveMax = ImGui::GetItemRectMax();
        const bool bSaveHovered = bCanSave && ImGui::IsItemHovered();
        const ImU32 SaveBg = ImGui::GetColorU32(bSaveHovered ? ImVec4(0.22f, 0.25f, 0.32f, 1.0f) : ImVec4(0.15f, 0.17f, 0.21f, 1.0f));
        const ImU32 SaveBorder = ImGui::GetColorU32(bCanSave ? ImVec4(0.40f, 0.44f, 0.58f, 1.0f) : ImVec4(0.29f, 0.31f, 0.36f, 1.0f));
        DrawList->AddRectFilled(SaveMin, SaveMax, SaveBg, 5.0f);
        DrawList->AddRect(SaveMin, SaveMax, SaveBorder, 5.0f);
        if (IconResources.SaveIcon)
        {
            DrawList->AddImage(
                reinterpret_cast<ImTextureID>(IconResources.SaveIcon),
                ImVec2(SaveMin.x + 5.0f, SaveMin.y + 5.0f),
                ImVec2(SaveMax.x - 5.0f, SaveMax.y - 5.0f),
                ImVec2(0.0f, 0.0f),
                ImVec2(1.0f, 1.0f),
                ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, bCanSave ? 1.0f : 0.45f)));
        }
        else
        {
            const ImU32 IconColor = ImGui::GetColorU32(ImVec4(0.84f, 0.88f, 0.96f, bCanSave ? 1.0f : 0.45f));
            DrawList->AddRect(ImVec2(SaveMin.x + 8.0f, SaveMin.y + 7.0f), ImVec2(SaveMax.x - 8.0f, SaveMax.y - 7.0f), IconColor, 2.0f, 0, 1.7f);
            DrawList->AddLine(ImVec2(SaveMin.x + 11.0f, SaveMax.y - 12.0f), ImVec2(SaveMax.x - 11.0f, SaveMax.y - 12.0f), IconColor, 1.7f);
        }
        if (bSaveHovered)
        {
            ImGui::SetTooltip("Save Scene");
        }
        if (bSaveClicked && bCanSave)
        {
            RequestSaveScene();
        }
        if (!bCanSave)
        {
            ImGui::EndDisabled();
        }

        if (!bCanPlaceActor)
        {
            ImGui::BeginDisabled();
        }

        ImGui::SetCursorPos(ImVec2(46.0f, 5.0f));
        const bool bAddClicked = ImGui::InvisibleButton("##ToolbarAddActor", ImVec2(ButtonSize, ButtonSize));
        const ImVec2 AddMin = ImGui::GetItemRectMin();
        const ImVec2 AddMax = ImGui::GetItemRectMax();
        const bool bAddHovered = bCanPlaceActor && ImGui::IsItemHovered();
        const ImU32 AddBg = ImGui::GetColorU32(bAddHovered ? ImVec4(0.22f, 0.25f, 0.32f, 1.0f) : ImVec4(0.15f, 0.17f, 0.21f, 1.0f));
        const ImU32 AddBorder = ImGui::GetColorU32(bCanPlaceActor ? ImVec4(0.40f, 0.44f, 0.58f, 1.0f) : ImVec4(0.29f, 0.31f, 0.36f, 1.0f));
        DrawList->AddRectFilled(AddMin, AddMax, AddBg, 5.0f);
        DrawList->AddRect(AddMin, AddMax, AddBorder, 5.0f);
        if (IconResources.AddActorIcon)
        {
            DrawList->AddImage(
                reinterpret_cast<ImTextureID>(IconResources.AddActorIcon),
                ImVec2(AddMin.x + 5.0f, AddMin.y + 5.0f),
                ImVec2(AddMax.x - 5.0f, AddMax.y - 5.0f));
        }
        else
        {
            const ImU32 IconColor = ImGui::GetColorU32(ImVec4(0.84f, 0.88f, 0.96f, 1.0f));
            const float Cx = (AddMin.x + AddMax.x) * 0.5f;
            const float Cy = (AddMin.y + AddMax.y) * 0.5f;
            DrawList->AddLine(ImVec2(Cx - 7.0f, Cy), ImVec2(Cx + 7.0f, Cy), IconColor, 2.0f);
            DrawList->AddLine(ImVec2(Cx, Cy - 7.0f), ImVec2(Cx, Cy + 7.0f), IconColor, 2.0f);
        }
        if (bAddHovered)
        {
            ImGui::SetTooltip("Place Actor");
        }
        if (bAddClicked)
        {
            ImGui::OpenPopup("##ToolbarPlaceActorPopup");
        }
        if (ImGui::BeginPopup("##ToolbarPlaceActorPopup"))
        {
            Widgets.ControlWidget.DrawPlaceActorMenu(ToolbarSpawnLocation);
            ImGui::EndPopup();
        }

        if (!bCanPlaceActor)
        {
            ImGui::EndDisabled();
        }

        const EViewportPlayState CurrentState = EditorEngine->GetEditorState();
        const bool bPlaying = CurrentState == EViewportPlayState::Playing;
        const bool bPaused = CurrentState == EViewportPlayState::Paused;
        const bool bCanStop = CurrentState != EViewportPlayState::Editing;
        const float CenterX = ToolbarSize.x * 0.5f;
        const float ButtonY = 5.0f;
        const float Gap = 8.0f;

        ImGui::SetCursorPos(ImVec2(CenterX - ButtonSize - Gap * 0.5f, ButtonY));
        const bool bPlayClicked = ImGui::InvisibleButton("##ToolbarPlayPause", ImVec2(ButtonSize, ButtonSize));
        const ImVec2 PlayMin = ImGui::GetItemRectMin();
        const ImVec2 PlayMax = ImGui::GetItemRectMax();
        const bool bPlayHovered = ImGui::IsItemHovered();
        DrawList->AddRectFilled(PlayMin, PlayMax, ImGui::GetColorU32(bPlayHovered ? ImVec4(0.18f, 0.34f, 0.22f, 1.0f) : ImVec4(0.14f, 0.24f, 0.17f, 1.0f)), 5.0f);
        DrawList->AddRect(PlayMin, PlayMax, ImGui::GetColorU32(ImVec4(0.34f, 0.62f, 0.39f, 1.0f)), 5.0f);
        const ImU32 PlayColor = ImGui::GetColorU32(ImVec4(0.52f, 0.95f, 0.60f, 1.0f));
        if (bPlaying)
        {
            DrawList->AddRectFilled(ImVec2(PlayMin.x + 9.0f, PlayMin.y + 8.0f), ImVec2(PlayMin.x + 13.0f, PlayMax.y - 8.0f), PlayColor, 1.0f);
            DrawList->AddRectFilled(ImVec2(PlayMax.x - 13.0f, PlayMin.y + 8.0f), ImVec2(PlayMax.x - 9.0f, PlayMax.y - 8.0f), PlayColor, 1.0f);
        }
        else
        {
            DrawList->AddTriangleFilled(
                ImVec2(PlayMin.x + 11.0f, PlayMin.y + 8.0f),
                ImVec2(PlayMin.x + 11.0f, PlayMax.y - 8.0f),
                ImVec2(PlayMax.x - 8.0f, (PlayMin.y + PlayMax.y) * 0.5f),
                PlayColor);
        }
        if (bPlayHovered)
        {
            ImGui::SetTooltip(bPlaying ? "Pause" : (bPaused ? "Resume" : "Play"));
        }
        if (bPlayClicked)
        {
            if (bPlaying)
            {
                EditorEngine->PausePlaySession();
            }
            else
            {
                EditorEngine->StartPlaySession();
            }
        }

        ImGui::SetCursorPos(ImVec2(CenterX + Gap * 0.5f, ButtonY));
        if (!bCanStop)
        {
            ImGui::BeginDisabled();
        }
        const bool bStopClicked = ImGui::InvisibleButton("##ToolbarStop", ImVec2(ButtonSize, ButtonSize));
        const ImVec2 StopMin = ImGui::GetItemRectMin();
        const ImVec2 StopMax = ImGui::GetItemRectMax();
        const bool bStopHovered = bCanStop && ImGui::IsItemHovered();
        DrawList->AddRectFilled(StopMin, StopMax, ImGui::GetColorU32(bStopHovered ? ImVec4(0.34f, 0.18f, 0.18f, 1.0f) : ImVec4(0.24f, 0.14f, 0.14f, 1.0f)), 5.0f);
        DrawList->AddRect(StopMin, StopMax, ImGui::GetColorU32(bCanStop ? ImVec4(0.65f, 0.34f, 0.34f, 1.0f) : ImVec4(0.35f, 0.30f, 0.30f, 1.0f)), 5.0f);
        DrawList->AddRectFilled(ImVec2(StopMin.x + 9.0f, StopMin.y + 9.0f), ImVec2(StopMax.x - 9.0f, StopMax.y - 9.0f), ImGui::GetColorU32(ImVec4(0.95f, 0.53f, 0.53f, bCanStop ? 1.0f : 0.45f)), 1.0f);
        if (bStopHovered)
        {
            ImGui::SetTooltip("Stop");
        }
        if (bStopClicked && bCanStop)
        {
            EditorEngine->StopPlaySession();
        }
        if (!bCanStop)
        {
            ImGui::EndDisabled();
        }

        ImGui::SetCursorPos(ImVec2(CenterX + ButtonSize + Gap * 1.5f, ButtonY));
        const bool bFullscreenOn = IsPIEViewportFullscreenEnabled();
        const bool bFullscreenClicked = ImGui::InvisibleButton("##ToolbarPIEFullscreen", ImVec2(ButtonSize, ButtonSize));
        const ImVec2 FullMin = ImGui::GetItemRectMin();
        const ImVec2 FullMax = ImGui::GetItemRectMax();
        const bool bFullHovered = ImGui::IsItemHovered();
        const ImVec4 FullBgColor = bFullscreenOn
            ? (bFullHovered ? ImVec4(0.20f, 0.31f, 0.46f, 1.0f) : ImVec4(0.15f, 0.24f, 0.37f, 1.0f))
            : (bFullHovered ? ImVec4(0.24f, 0.26f, 0.31f, 1.0f) : ImVec4(0.18f, 0.20f, 0.24f, 1.0f));
        DrawList->AddRectFilled(FullMin, FullMax, ImGui::GetColorU32(FullBgColor), 5.0f);
        DrawList->AddRect(FullMin, FullMax, ImGui::GetColorU32(bFullscreenOn ? ImVec4(0.36f, 0.57f, 0.86f, 1.0f) : ImVec4(0.38f, 0.40f, 0.46f, 1.0f)), 5.0f);
        const ImU32 FullIconColor = ImGui::GetColorU32(bFullscreenOn ? ImVec4(0.64f, 0.82f, 1.00f, 1.0f) : ImVec4(0.72f, 0.76f, 0.84f, 1.0f));
        const float Pad = 8.0f;
        const float Corner = 6.0f;
        DrawList->AddLine(ImVec2(FullMin.x + Pad, FullMin.y + Pad), ImVec2(FullMin.x + Pad + Corner, FullMin.y + Pad), FullIconColor, 1.8f);
        DrawList->AddLine(ImVec2(FullMin.x + Pad, FullMin.y + Pad), ImVec2(FullMin.x + Pad, FullMin.y + Pad + Corner), FullIconColor, 1.8f);
        DrawList->AddLine(ImVec2(FullMax.x - Pad, FullMin.y + Pad), ImVec2(FullMax.x - Pad - Corner, FullMin.y + Pad), FullIconColor, 1.8f);
        DrawList->AddLine(ImVec2(FullMax.x - Pad, FullMin.y + Pad), ImVec2(FullMax.x - Pad, FullMin.y + Pad + Corner), FullIconColor, 1.8f);
        DrawList->AddLine(ImVec2(FullMin.x + Pad, FullMax.y - Pad), ImVec2(FullMin.x + Pad + Corner, FullMax.y - Pad), FullIconColor, 1.8f);
        DrawList->AddLine(ImVec2(FullMin.x + Pad, FullMax.y - Pad), ImVec2(FullMin.x + Pad, FullMax.y - Pad - Corner), FullIconColor, 1.8f);
        DrawList->AddLine(ImVec2(FullMax.x - Pad, FullMax.y - Pad), ImVec2(FullMax.x - Pad - Corner, FullMax.y - Pad), FullIconColor, 1.8f);
        DrawList->AddLine(ImVec2(FullMax.x - Pad, FullMax.y - Pad), ImVec2(FullMax.x - Pad, FullMax.y - Pad - Corner), FullIconColor, 1.8f);
        if (bFullHovered)
        {
            ImGui::SetTooltip(bFullscreenOn ? "PIE Fullscreen Viewport: On" : "PIE Fullscreen Viewport: Off");
        }
        if (bFullscreenClicked)
        {
            SetPIEViewportFullscreenEnabled(!bFullscreenOn);
        }

        if (bCanStop)
        {
            FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
            const int32 ActivePIEViewportIndex = EditorEngine->GetPIESession().GetActiveViewportIndex();
            const int32 PIEViewportIndex = (ActivePIEViewportIndex >= 0)
                ? ActivePIEViewportIndex
                : Layout.GetLastFocusedViewportIndex();
            FEditorViewportClient* PIEClient = Layout.GetViewportClient(PIEViewportIndex);

            ImGui::SetCursorPos(ImVec2(FullMax.x - ImGui::GetWindowPos().x + Gap, ButtonY + 2.0f));
            if (PIEClient)
            {
                char PIEViewPopupID[48];
                snprintf(PIEViewPopupID, sizeof(PIEViewPopupID), "##MainToolbarPIEViewPopup_%d", PIEViewportIndex);
                char PIEViewButtonLabel[80];
                snprintf(PIEViewButtonLabel, sizeof(PIEViewButtonLabel), "View: %s", GetMainToolbarViewModeName(PIEClient->GetViewportState()->ViewMode));

                if (ImGui::Button(PIEViewButtonLabel, ImVec2(132.0f, ButtonSize - 4.0f)))
                {
                    ImGui::OpenPopup(PIEViewPopupID);
                }
                if (ImGui::BeginPopup(PIEViewPopupID))
                {
                    static constexpr EViewMode PIEViewModes[] = {
                        EViewMode::Lit_Gouraud,
                        EViewMode::Unlit,
                        EViewMode::Wireframe,
                        EViewMode::Depth,
                        EViewMode::Normal,
                    };
                    for (EViewMode Mode : PIEViewModes)
                    {
                        if (ImGui::MenuItem(GetMainToolbarViewModeName(Mode), nullptr, PIEClient->GetViewportState()->ViewMode == Mode))
                        {
                            PIEClient->GetViewportState()->ViewMode = Mode;
                        }
                    }
                    ImGui::EndPopup();
                }
                ImGui::SameLine(0.0f, Gap);
            }

            if (ImGui::Checkbox("PIE 1920x1080", &PIEViewportState.bUseFixedUILayout) && PIEViewportState.bUseFixedUILayout)
            {
                PIEViewportState.UILayoutWidth = DefaultPIEUILayoutWidth;
                PIEViewportState.UILayoutHeight = DefaultPIEUILayoutHeight;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Use a 16:9 PIE viewport and a fixed 1920x1080 RML layout.");
            }
        }
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}
