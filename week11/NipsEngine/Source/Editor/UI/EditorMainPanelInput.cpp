#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/ViewportLayout.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include <cstring>
#include <imm.h>

void FEditorMainPanel::Update()
{
    ImGuiIO& IO = ImGui::GetIO();

    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    const bool bMouseOverContentBrowser = Widgets.ContentBrowserWidget.IsMouseOverBrowser();
    bool bViewportOperationActive = Layout.HasActiveOperationViewport() && !bMouseOverContentBrowser;

    if (bViewportOperationActive)
    {
        IO.ConfigFlags |= ImGuiConfigFlags_NoMouse;
    }
    else
    {
        IO.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    }

    bool bWantMouse = bViewportOperationActive ? false : IO.WantCaptureMouse;
    bool bWantKeyboard = IO.WantCaptureKeyboard;
    const bool bWantTextInput = IO.WantTextInput;
    const bool bAnyUIItemActive = ImGui::IsAnyItemActive();
    const bool bAnyUIItemHovered = ImGui::IsAnyItemHovered();
    const bool bAnyPopupOpen = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
    const bool bAnyDragDropActive = ImGui::GetDragDropPayload() != nullptr;
    const bool bAnyWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
    const bool bAnyWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow);

    bool bMouseOverViewportRect = false;
    if (Window)
    {
        POINT MouseClientPos = Window->ScreenToClientPoint(InputSystem::Get().GetMousePos());
        for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
        {
            const FViewportRect& ViewportRect = Layout.GetSceneViewport(i).GetRect();
            if (ViewportRect.Width > 0 && ViewportRect.Height > 0 && ViewportRect.Contains(MouseClientPos.x, MouseClientPos.y))
            {
                bMouseOverViewportRect = true;
                break;
            }
        }

		TArray<std::unique_ptr<FEditorViewer>>& Viewers = EditorEngine->GetViewers();
		for (size_t i = 0; i < Viewers.size(); i++)
		{
            const FViewportRect& ViewportRect = Viewers[i]->GetViewport().GetRect();
            if (ViewportRect.Width > 0 && ViewportRect.Height > 0 && ViewportRect.Contains(MouseClientPos.x, MouseClientPos.y))
            {
                bMouseOverViewportRect = true;
                break;
            }
		}
    }

    bool bHoveredViewportContentWindow = false;
    bool bHoveredNonViewportWindow = false;
    if (ImGuiContext* Context = ImGui::GetCurrentContext())
    {
        if (ImGuiWindow* HoveredWindow = Context->HoveredWindow)
        {
            const char* HoveredName = HoveredWindow->Name ? HoveredWindow->Name : "";
            bHoveredViewportContentWindow =
                (std::strcmp(HoveredName, "Viewport") == 0)
                || (std::strncmp(HoveredName, "Viewport###", 11) == 0)
                || (std::strcmp(HoveredName, "Viewer") == 0)
                || (std::strncmp(HoveredName, "Viewer/ViewportPanel###", 9) == 0);
            bHoveredNonViewportWindow = !bHoveredViewportContentWindow;
        }
    }

    if (!bHoveredViewportContentWindow
        && bMouseOverViewportRect
        && !bHoveredNonViewportWindow
        && !bMouseOverContentBrowser
        && !bAnyUIItemActive
        && !bAnyPopupOpen)
    {
        bHoveredViewportContentWindow = true;
    }

    if (bMouseOverContentBrowser)
    {
        bHoveredViewportContentWindow = false;
        bHoveredNonViewportWindow = true;
    }

    const bool bForcePIEViewportInputFocus =
        PIEViewportState.PendingInputFocusFrames > 0
        && EditorEngine
        && EditorEngine->GetEditorState() == EViewportPlayState::Playing;

    const bool bReleaseMouseToViewport =
        bMouseOverViewportRect
        && !bHoveredNonViewportWindow
        && !bAnyUIItemActive
        && !bAnyDragDropActive
        && !bAnyPopupOpen;
    const bool bNonViewportImGuiInteraction =
        bMouseOverContentBrowser
        ||
        (bHoveredNonViewportWindow && (bAnyWindowHovered || bAnyWindowFocused || bAnyUIItemActive || bAnyUIItemHovered || bAnyPopupOpen || bWantTextInput || bWantKeyboard))
        || bAnyUIItemActive
        || bAnyDragDropActive
        || bAnyPopupOpen;

    if (bNonViewportImGuiInteraction)
    {
        bWantMouse = true;
    }
    else if (EditorEngine && bReleaseMouseToViewport)
    {
        bWantMouse = false;
        bWantKeyboard = false;
    }

    if (bForcePIEViewportInputFocus)
    {
        bWantMouse = false;
        bWantKeyboard = false;
    }

    InputSystem::Get().SetGuiMouseCapture(bWantMouse);
    InputSystem::Get().SetGuiKeyboardCapture(bWantKeyboard);
    InputSystem::Get().SetGuiTextInputCapture(bForcePIEViewportInputFocus ? false : bWantTextInput);
    const bool bAllowViewportMouseFocus =
        (bForcePIEViewportInputFocus || bMouseOverViewportRect) &&
        !bHoveredNonViewportWindow &&
        !bAnyPopupOpen &&
        !bAnyDragDropActive &&
        !bWantTextInput;
    InputSystem::Get().SetGuiViewportMouseBlock(
        bForcePIEViewportInputFocus
            ? false
            : (bAnyDragDropActive ||
               bAnyPopupOpen ||
               bMouseOverContentBrowser ||
               bHoveredNonViewportWindow));
    InputSystem::Get().SetGuiViewportMouseFocusAllowed(bAllowViewportMouseFocus);

    if (bForcePIEViewportInputFocus)
    {
        --PIEViewportState.PendingInputFocusFrames;
    }

    if (EditorEngine && InputSystem::Get().GetKeyUp('F') && !IO.WantTextInput)
    {
        FEditorViewportLayout& FocusLayout = EditorEngine->GetViewportLayout();
        const int32 FocusedIdx = FocusLayout.GetLastFocusedViewportIndex();
        FocusLayout.GetViewportClient(FocusedIdx)->FocusSelection();
    }

    if (Window)
    {
        HWND hWnd = Window->GetHWND();
        if (IO.WantTextInput)
        {
            ImmAssociateContextEx(hWnd, NULL, IACE_DEFAULT);
        }
        else
        {
            ImmAssociateContext(hWnd, NULL);
        }
    }
}
