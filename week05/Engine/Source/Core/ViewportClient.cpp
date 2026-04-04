#include "ViewportClient.h"

#include "Camera/Camera.h"
#include "Component/CameraComponent.h"
#include "Core/Engine.h"
#include "Debug/EngineLog.h"
#include "Input/InputManager.h"
#include "Math/Frustum.h"
#include "Renderer/Material.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Renderer.h"
#include "Scene/Scene.h"
#include "World/World.h"
#include <utility>

void IViewportClient::Attach(FEngine* Engine, FRenderer* Renderer)
{
}

void IViewportClient::Detach(FEngine* Engine, FRenderer* Renderer)
{
}

void IViewportClient::Tick(FEngine* Engine, float DeltaTime)
{
}

void IViewportClient::HandleMessage(FEngine* Engine, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
}

UScene* IViewportClient::ResolveScene(FEngine* Engine) const
{
	return Engine ? Engine->GetActiveScene() : nullptr;
}

UWorld* IViewportClient::ResolveWorld(FEngine* Engine) const
{
	return Engine ? Engine->GetActiveWorld() : nullptr;
}

void IViewportClient::BuildRenderCommands(FEngine* Engine, UScene* Scene, const FFrustum& Frustum, const FShowFlags& Flags, const FVector& CameraPosition, FRenderCommandQueue& OutQueue)
{
	(void)Scene;
	OutQueue.ShowFlags = Flags;

	UWorld* World = ResolveWorld(Engine);
	if (!World)
	{
		return;
	}

	if (UScene* PersistentLevel = World->GetPersistentLevel())
	{
		RenderCollector.CollectRenderCommands(PersistentLevel, Frustum, Flags, CameraPosition, OutQueue);
	}

	for (UScene* StreamingLevel : World->GetStreamingLevels())
	{
		if (!StreamingLevel)
		{
			continue;
		}

		RenderCollector.CollectRenderCommands(StreamingLevel, Frustum, Flags, CameraPosition, OutQueue);
	}
}

void IViewportClient::HandleFileDoubleClick(const FString& FilePath)
{
}

void IViewportClient::HandleFileDropOnViewport(const FString& FilePath)
{
}

void IViewportClient::Render(FEngine* Engine, FRenderer* Renderer)
{
}

void FGameViewportClient::Attach(FEngine* Engine, FRenderer* Renderer)
{
	if (Renderer)
	{
		Renderer->ClearViewportCallbacks();
	}
}

void FGameViewportClient::Detach(FEngine* Engine, FRenderer* Renderer)
{
	if (Renderer)
	{
		Renderer->ClearViewportCallbacks();
	}
}

void FGameViewportClient::Render(FEngine* Engine, FRenderer* Renderer)
{
	if (!Engine || !Renderer)
	{
		return;
	}

	UScene* Scene = ResolveScene(Engine);
	if (!Scene)
	{
		return;
	}

	UWorld* ActiveWorld = ResolveWorld(Engine);
	if (!ActiveWorld)
	{
		return;
	}

	UCameraComponent* ActiveCamera = ActiveWorld->GetActiveCameraComponent();
	if (!ActiveCamera)
	{
		return;
	}

	FRenderCommandQueue Queue;
	Queue.Reserve(Renderer->GetPrevCommandCount());
	Queue.ViewMatrix = ActiveCamera->GetViewMatrix();
	Queue.ProjectionMatrix = ActiveCamera->GetProjectionMatrix();

	FFrustum Frustum;
	Frustum.ExtractFromVP(Queue.ViewMatrix * Queue.ProjectionMatrix);

	const FMatrix ViewInverse = Queue.ViewMatrix.GetInverse();
	const FVector CameraPosition = ViewInverse.GetTranslation();
	BuildRenderCommands(Engine, Scene, Frustum, FShowFlags{}, CameraPosition, Queue);
	Renderer->SubmitCommands(std::move(Queue));
	Renderer->ExecuteCommands();
}
