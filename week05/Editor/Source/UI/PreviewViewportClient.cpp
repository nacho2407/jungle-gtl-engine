#include "PreviewViewportClient.h"

#include "EditorUI.h"
#include "Core/Engine.h"
#include "Platform/Windows/WindowsWindow.h"
#include "Renderer/Renderer.h"
#include "imgui.h"

CPreviewViewportClient::CPreviewViewportClient(CEditorUI& InEditorUI, FWindowsWindow* InMainWindow, FString InPreviewContextName)
	: EditorUI(InEditorUI)
	, MainWindow(InMainWindow)
	, PreviewContextName(std::move(InPreviewContextName))
{
}

void CPreviewViewportClient::Attach(FEngine* Engine, CRenderer* Renderer)
{
	if (!Engine || !Renderer || !MainWindow)
	{
		return;
	}

	EditorUI.Initialize(Engine);
	EditorUI.SetupWindow(MainWindow);
	EditorUI.AttachToRenderer(Renderer);
}

void CPreviewViewportClient::Detach(FEngine* Engine, CRenderer* Renderer)
{
	EditorUI.DetachFromRenderer(Renderer);
}

void CPreviewViewportClient::Tick(FEngine* Engine, float DeltaTime)
{
	if (!Engine)
	{
		return;
	}

	if (ImGui::GetCurrentContext())
	{
		const ImGuiIO& IO = ImGui::GetIO();
		if ((IO.WantCaptureKeyboard || IO.WantCaptureMouse) && !EditorUI.IsViewportInteractive())
		{
			return;
		}
	}

	if (!EditorUI.IsViewportInteractive())
	{
		return;
	}

	IViewportClient::Tick(Engine, DeltaTime);
}

UScene* CPreviewViewportClient::ResolveScene(FEngine* Engine) const
{
	if (!Engine)
	{
		return nullptr;
	}

	if (UScene* PreviewScene = Engine->GetPreviewScene(PreviewContextName))
	{
		return PreviewScene;
	}

	return Engine->GetActiveScene();
}
