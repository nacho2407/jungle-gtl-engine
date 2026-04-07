#include "ViewportClient.h"
#include "World/World.h"
#include "Input/InputManager.h"
#include "Camera/Camera.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Material.h"
#include "Scene/Scene.h"
#include "Debug/EngineLog.h"
#include "Component/UUIDBillboardComponent.h"
#include "Component/SubUVComponent.h"
#include "Core/Engine.h"
#include "Component/TextComponent.h"
#include "Component/CameraComponent.h"
#include "Math/Frustum.h"


void IViewportClient::Attach(FEngine* Engine, FRenderer* Renderer)
{
}

void IViewportClient::Detach(FEngine* Engine, FRenderer* Renderer)
{
}

void IViewportClient::Tick(FEngine* Engine, float DeltaTime)
{
	// instead Enhance input system controller
	//if (!Core)
	//{
	//	return;
	//}

	//FInputManager* InputManager = Core->GetInputManager();
	//UScene* Scene = ResolveScene(Core);
	//if (!InputManager || !Scene)
	//{
	//	return;
	//}

	//FCamera* Camera = Scene->GetCamera();
	//if (!Camera)
	//{
	//	return;
	//}

	//if (InputManager->IsKeyDown('W')) Camera->MoveForward(DeltaTime);
	//if (InputManager->IsKeyDown('S')) Camera->MoveForward(-DeltaTime);
	//if (InputManager->IsKeyDown('D')) Camera->MoveRight(DeltaTime);
	//if (InputManager->IsKeyDown('A')) Camera->MoveRight(-DeltaTime);
	//if (InputManager->IsKeyDown('E')) Camera->MoveUp(DeltaTime);
	//if (InputManager->IsKeyDown('Q')) Camera->MoveUp(-DeltaTime);

	//if (InputManager->IsMouseButtonDown(FInputManager::MOUSE_RIGHT))
	//{
	//	const float DeltaX = InputManager->GetMouseDeltaX();
	//	const float DeltaY = InputManager->GetMouseDeltaY();
	//	Camera->Rotate(DeltaX * 0.2f, -DeltaY * 0.2f);
	//}
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
	UWorld* World = ResolveWorld(Engine);
	if (!World) return;

	// Persistent + Streaming 전체 액터를 렌더
	TArray<AActor*> AllActors = World->GetAllActors();
	RenderCollector.CollectRenderCommands(AllActors, Frustum, Flags, CameraPosition, OutQueue);
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

	const FVector CameraPosition = Queue.ViewMatrix.GetInverse().GetTranslation();
	BuildRenderCommands(Engine, Scene, Frustum, FShowFlags{}, CameraPosition, Queue);
	Renderer->SubmitCommands(Queue);
	Renderer->ExecuteCommands();
}
