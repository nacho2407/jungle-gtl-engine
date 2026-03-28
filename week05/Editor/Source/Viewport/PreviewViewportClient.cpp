#include "PreviewViewportClient.h"

#include "UI/EditorUI.h"
#include "Core/Core.h"
#include "Platform/Windows/Window.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderCommand.h"
#include "Component/CameraComponent.h"
#include "Math/Frustum.h"
#include "World/World.h"
#include "imgui.h"

CPreviewViewportClient::CPreviewViewportClient(CEditorUI& InEditorUI, CWindow* InMainWindow, FString InPreviewContextName)
	: EditorUI(InEditorUI)
	, MainWindow(InMainWindow)
	, PreviewContextName(std::move(InPreviewContextName))
{
}

void CPreviewViewportClient::Attach(CCore* Core, CRenderer* Renderer)
{
	if (!Core || !Renderer || !MainWindow)
	{
		return;
	}

	EditorUI.Initialize(Core);
	EditorUI.SetupWindow(MainWindow);
	EditorUI.AttachToRenderer(Renderer);
}

void CPreviewViewportClient::Detach(CCore* Core, CRenderer* Renderer)
{
	EditorUI.DetachFromRenderer(Renderer);
}

void CPreviewViewportClient::Tick(CCore* Core, float DeltaTime)
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

void CPreviewViewportClient::Render(CCore* Core, CRenderer* Renderer)
{
	if (!Core || !Renderer)
	{
		return;
	}

	UScene* Scene = ResolveScene(Core);
	UWorld* ActiveWorld = ResolveWorld(Core);

	if (Scene && ActiveWorld)
	{
		UCameraComponent* ActiveCamera = ActiveWorld->GetActiveCameraComponent();
		if (ActiveCamera)
		{
			FRenderCommandQueue Queue;
			Queue.Reserve(Renderer->GetPrevCommandCount());
			Queue.ViewMatrix = ActiveCamera->GetViewMatrix();
			Queue.ProjectionMatrix = ActiveCamera->GetProjectionMatrix();

			FFrustum Frustum;
			Frustum.ExtractFromVP(Queue.ViewMatrix * Queue.ProjectionMatrix);

			BuildRenderCommands(Core, Scene, Frustum, Queue);
			Renderer->SubmitCommands(Queue);
			Renderer->ExecuteCommands();
		}
	}

	EditorUI.Render();
}

UScene* CPreviewViewportClient::ResolveScene(CCore* Core) const
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

