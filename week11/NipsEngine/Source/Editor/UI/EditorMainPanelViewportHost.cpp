#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Viewport/ViewportLayout.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Render/Renderer/Renderer.h"

#include "ImGui/imgui.h"

#include <cstdio>
#include <cmath>

namespace
{
void SetOpaqueBlendStateCallback(const ImDrawList*, const ImDrawCmd* Cmd)
{
    ID3D11DeviceContext* DeviceContext = static_cast<ID3D11DeviceContext*>(Cmd->UserCallbackData);
    if (!DeviceContext)
        return;

    const float BlendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
    DeviceContext->OMSetBlendState(nullptr, BlendFactor, 0xffffffff);
}

bool UsesAbsoluteImGuiCoordinates()
{
    return (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0;
}

POINT ImGuiScreenToClientPoint(FWindowsWindow* Window, const ImVec2& Point)
{
    POINT Result =
    {
        static_cast<LONG>(std::lround(Point.x)),
        static_cast<LONG>(std::lround(Point.y))
    };
    if (Window && Window->GetHWND() && UsesAbsoluteImGuiCoordinates())
    {
        ::ScreenToClient(Window->GetHWND(), &Result);
    }
    return Result;
}

ImVec2 ClientToImGuiScreenPoint(FWindowsWindow* Window, int32 X, int32 Y)
{
    POINT Result =
    {
        static_cast<LONG>(X),
        static_cast<LONG>(Y)
    };
    if (Window && Window->GetHWND() && UsesAbsoluteImGuiCoordinates())
    {
        ::ClientToScreen(Window->GetHWND(), &Result);
    }
    return ImVec2(static_cast<float>(Result.x), static_cast<float>(Result.y));
}
} // namespace

void FEditorMainPanel::RenderViewportHostWindow()
{
    if (!EditorEngine)
        return;
    constexpr ImGuiWindowFlags WindowFlags = 0;
    FGuiInputState& GuiState = InputSystem::Get().GetGuiInputState();

    if (!ImGui::Begin("Viewport", nullptr, WindowFlags))
    {
        GuiState.bViewportHostVisible = false;
        GuiState.ViewportHostRect = FViewportRect();
        EditorEngine->GetViewportLayout().SetHostRect(FViewportRect());
        ImGui::End();
        return;
    }

    const ImVec2 ContentSize = ImGui::GetContentRegionAvail();
    if (ContentSize.x > 1.0f && ContentSize.y > 1.0f)
    {
        const ImVec2 ContentPos = ImGui::GetCursorScreenPos();
        const POINT ContentClientPos = ImGuiScreenToClientPoint(Window, ContentPos);
        const FViewportRect HostRect(
            static_cast<int32>(ContentClientPos.x),
            static_cast<int32>(ContentClientPos.y),
            static_cast<int32>(ContentSize.x),
            static_cast<int32>(ContentSize.y));

        GuiState.bViewportHostVisible = true;
        GuiState.ViewportHostRect = HostRect;
        FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
        const bool bPIEActive = EditorEngine->GetEditorState() != EViewportPlayState::Editing;
        const int32 ActivePIEViewportIndex = EditorEngine->GetPIESession().GetActiveViewportIndex();
        for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
        {
            const FEditorViewportClient* Client = Layout.GetViewportClient(i);
            const bool bHideToolbar = bPIEActive && Client && Client->IsPIEPossessed();
            Layout.SetViewportChromeTopInset(
                i,
                bHideToolbar ? 0 : FEditorViewportLayout::DefaultViewportToolbarHeight);
        }

        Layout.SetHostRect(HostRect);
        ApplyPIEFixedAspectViewportRect();

        const int32 FocusedViewportIndex = Layout.GetLastFocusedViewportIndex();
        auto DrawSceneViewport = [&](int32 ViewportIndex)
        {
            if (bPIEActive && ActivePIEViewportIndex >= 0 && ViewportIndex != ActivePIEViewportIndex)
            {
                return;
            }

            auto& VP = Layout.GetSceneViewport(ViewportIndex);
            const FViewportRect ViewportRect = VP.GetRect();
            if (ViewportRect.Width <= 0 || ViewportRect.Height <= 0)
            {
                return;
            }

            const ID3D11ShaderResourceView* SceneColorSRV = VP.GetOutSRV();

            ImVec2 Size = ImVec2(
                static_cast<float>(ViewportRect.Width),
                static_cast<float>(ViewportRect.Height));
            const ImVec2 ViewportScreenPos = ClientToImGuiScreenPoint(Window, ViewportRect.X, ViewportRect.Y);
            ImGui::SetCursorScreenPos(ViewportScreenPos);
            ImDrawList* DrawList = ImGui::GetWindowDrawList();

            if (SceneColorSRV)
            {
                ID3D11DeviceContext* DeviceContext = EditorEngine->GetRenderer().GetFD3DDevice().GetDeviceContext();

                DrawList->AddCallback(SetOpaqueBlendStateCallback, DeviceContext);
                ImGui::Image(reinterpret_cast<ImTextureID>(SceneColorSRV), Size);
                DrawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
            }
            else
            {
                ImGui::Dummy(Size);
            }

            if (bPIEActive && ViewportIndex == FocusedViewportIndex)
            {
                RenderRuntimeUIForPIEViewport(ViewportRect, ImGui::GetIO().DeltaTime);
            }

            FEditorViewportClient* DropClient = Layout.GetViewportClient(ViewportIndex);
            const FEditorViewportState& State = Layout.GetViewportState(ViewportIndex);
            const float PIEFlashAlpha = DropClient ? DropClient->GetPIEStartOutlineFlashAlpha() : 0.0f;
            const bool bFocused = ViewportIndex == FocusedViewportIndex;
            const bool bHovered = State.bHovered;
            if (bFocused || bHovered || PIEFlashAlpha > 0.0f)
            {
                const ImVec2 OutlineMin = ViewportScreenPos;
                const ImVec2 OutlineMax = ClientToImGuiScreenPoint(
                    Window,
                    ViewportRect.X + ViewportRect.Width,
                    ViewportRect.Y + ViewportRect.Height);
                const ImU32 OutlineColor = bFocused ? IM_COL32(82, 168, 255, 235) : IM_COL32(170, 190, 210, 120);
                DrawList->AddRect(OutlineMin, OutlineMax, OutlineColor, 0.0f, 0, bFocused ? 2.0f : 1.0f);
                if (PIEFlashAlpha > 0.0f)
                {
                    const int32 Alpha = static_cast<int32>(220.0f * PIEFlashAlpha);
                    DrawList->AddRect(OutlineMin, OutlineMax, IM_COL32(120, 255, 150, Alpha), 0.0f, 0, 4.0f);
                }
            }
        };

		for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
        {
            if (i != FocusedViewportIndex)
            {
                DrawSceneViewport(i);
            }
        }
        if (FocusedViewportIndex >= 0 && FocusedViewportIndex < FEditorViewportLayout::MaxViewports)
        {
            DrawSceneViewport(FocusedViewportIndex);
        }
        if (!bPIEActive)
        {
            Widgets.ViewportOverlayWidget.RenderSplitterBar(ImGui::GetWindowDrawList());
        }

        // Per-viewport menu overlay.
        {
            const bool bOnlyFocusedToolbar = Layout.IsLayoutTransitionActive();

            for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
            {
                const int32 DrawIndex = (i == FEditorViewportLayout::MaxViewports - 1)
                    ? FocusedViewportIndex
                    : (i < FocusedViewportIndex ? i : i + 1);
                if (DrawIndex < 0 || DrawIndex >= FEditorViewportLayout::MaxViewports)
                    continue;
                if (bPIEActive && ActivePIEViewportIndex >= 0 && DrawIndex != ActivePIEViewportIndex)
                    continue;
                if (bOnlyFocusedToolbar && DrawIndex != FocusedViewportIndex)
                    continue;

                if (FEditorViewportClient* Client = Layout.GetViewportClient(DrawIndex))
                {
                    const bool bHidePIEViewportToolbar =
                        EditorEngine->GetEditorState() != EViewportPlayState::Editing && Client->IsPIEPossessed();
                    Client->SetViewportInputDeadZoneTop(0.0f);
                    if (bHidePIEViewportToolbar)
                    {
                        continue;
                    }
                }

                FViewportRect ViewportRect = Layout.GetSceneViewport(DrawIndex).GetRect();
                if (ViewportRect.Width <= 0 || ViewportRect.Height <= 0)
                    continue;

                const int32 MenuBarHeight = Layout.GetViewportChromeTopInset(DrawIndex);
                if (MenuBarHeight <= 0)
                    continue;

                const int32 ToolbarY = ViewportRect.Y - MenuBarHeight;
                if (ViewportRect.X < HostRect.X || ToolbarY < HostRect.Y)
                    continue;

                ImGui::SetCursorScreenPos(ClientToImGuiScreenPoint(Window, ViewportRect.X, ToolbarY));

                char ChildID[32];
                snprintf(ChildID, sizeof(ChildID), "##VPMenu%d", DrawIndex);

                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 2.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.13f, 0.16f, 0.98f));
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.22f, 0.26f, 0.95f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.29f, 0.35f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.30f, 0.53f, 1.0f));
                constexpr ImGuiWindowFlags OverlayFlags =
                    ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse |
                    ImGuiWindowFlags_NoNav |
                    ImGuiWindowFlags_NoFocusOnAppearing;

                if (ImGui::BeginChild(ChildID, ImVec2(static_cast<float>(ViewportRect.Width), static_cast<float>(MenuBarHeight)), false, OverlayFlags))
                {
                    ImGui::PushID(DrawIndex);
                    RenderViewportIconToolbarForIndex(DrawIndex);
                    ImGui::PopID();
                }
                ImGui::EndChild();
                ImGui::PopStyleColor(4);
                ImGui::PopStyleVar(4);
            }
        }

        TickViewportContextMenu();
        RenderViewportContextMenu();
    }
    else
    {
        GuiState.bViewportHostVisible = false;
        GuiState.ViewportHostRect = FViewportRect();
        EditorEngine->GetViewportLayout().SetHostRect(FViewportRect());
    }

    ImGui::End();
}
