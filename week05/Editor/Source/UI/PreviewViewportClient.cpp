#include "PreviewViewportClient.h"

#include "EditorUI.h"
#include "Core/EngineRuntime.h"
#include "Platform/Windows/WindowsWindow.h"
#include "Renderer/Renderer.h"

#include "imgui.h"

CPreviewViewportClient::CPreviewViewportClient(CEditorUI& InEditorUI, FWindowsWindow* InMainWindow, FString InPreviewContextName)
	: EditorUI(InEditorUI)
	, MainWindow(InMainWindow)
	, PreviewContextName(std::move(InPreviewContextName))
{
}

void CPreviewViewportClient::Attach(FEngineRuntime* Core, CRenderer* Renderer)
{
	if (!Core || !Renderer || !MainWindow)
	{
		return;
	}

	EditorUI.Initialize(Core);
	EditorUI.SetupWindow(MainWindow);
	EditorUI.AttachToRenderer(Renderer);
}

void CPreviewViewportClient::Detach(FEngineRuntime* Core, CRenderer* Renderer)
{
	EditorUI.DetachFromRenderer(Renderer);
}

void CPreviewViewportClient::Tick(FEngineRuntime* Core, float DeltaTime)
{
	if (!Core)
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

	IViewportClient::Tick(Core, DeltaTime);
}

UScene* CPreviewViewportClient::ResolveScene(FEngineRuntime* Core) const
{
	if (!Core)
	{
		return nullptr;
	}

	if (UScene* PreviewScene = Core->GetSceneManager()->GetPreviewScene(PreviewContextName))
	{
		return PreviewScene;
	}

	return Core->GetActiveScene();
}
