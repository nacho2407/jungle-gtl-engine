#include "PreviewViewportClient.h"

#include "EditorEngine.h"
#include "EditorUI.h"
#include "Renderer/Renderer.h"
#include "imgui.h"

FPreviewViewportClient::FPreviewViewportClient(FEditorUI& InEditorUI, FString InPreviewContextName)
	: EditorUI(InEditorUI)
	, PreviewContextName(std::move(InPreviewContextName))
{
}

void FPreviewViewportClient::Attach(FEngine* Engine, FRenderer* Renderer)
{
	FEditorEngine* EditorEngine = static_cast<FEditorEngine*>(Engine);
	if (!EditorEngine || !Renderer)
	{
		return;
	}

	EditorUI.Initialize(EditorEngine);
	EditorUI.AttachToRenderer(Renderer);
}

void FPreviewViewportClient::Detach(FEngine* Engine, FRenderer* Renderer)
{
	EditorUI.DetachFromRenderer(Renderer);
}

void FPreviewViewportClient::Tick(FEngine* Engine, float DeltaTime)
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

UScene* FPreviewViewportClient::ResolveScene(FEngine* Engine) const
{
	FEditorEngine* EditorEngine = static_cast<FEditorEngine*>(Engine);
	if (!EditorEngine)
	{
		return nullptr;
	}

	if (UScene* PreviewScene = EditorEngine->GetPreviewScene(PreviewContextName))
	{
		return PreviewScene;
	}

	return EditorEngine->GetActiveScene();
}
