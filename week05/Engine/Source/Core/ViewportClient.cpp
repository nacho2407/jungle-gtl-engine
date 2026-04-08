#include "ViewportClient.h"
#include "World/World.h"
#include "Input/InputManager.h"
#include "Camera/Camera.h"
#include "Renderer/Renderer.h"
#include "Level/Level.h"
#include "Level/SceneRenderPacket.h"
#include "Debug/EngineLog.h"
#include "Core/Engine.h"
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
	// 예전 직접 카메라 입력 경로는 Enhanced Input 전환 이후 보류 상태다.
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

ULevel* IViewportClient::ResolveScene(FEngine* Engine) const
{
	return Engine ? Engine->GetActiveScene() : nullptr;
}

UWorld* IViewportClient::ResolveWorld(FEngine* Engine) const
{
	return Engine ? Engine->GetActiveWorld() : nullptr;
}

void IViewportClient::BuildSceneRenderPacket(
	FEngine* Engine,
	UWorld* World,
	const FFrustum& Frustum,
	const FShowFlags& Flags,
	FSceneRenderPacket& OutPacket)
{
	// ViewportClient는 렌더 커맨드를 만들지 않고, 월드를 씬 패킷으로만 변환한다.
	if (!World)
	{
		return;
	}

	TArray<AActor*> AllActors = World->GetAllActors();
	ScenePacketBuilder.BuildScenePacket(AllActors, Frustum, Flags, OutPacket);
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
	(void)Engine;
	(void)Renderer;
}

void FGameViewportClient::Detach(FEngine* Engine, FRenderer* Renderer)
{
	(void)Engine;
	(void)Renderer;
}

void FGameViewportClient::Render(FEngine* Engine, FRenderer* Renderer)
{
	if (!Engine || !Renderer)
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

	// 게임 뷰포트는 활성 카메라 기준으로 프러스텀을 만들고 씬 패킷을 채운다.
	FSceneRenderPacket ScenePacket;
	FGameFrameRequest FrameRequest;
	FrameRequest.SceneView.ViewMatrix = ActiveCamera->GetViewMatrix();
	FrameRequest.SceneView.ProjectionMatrix = ActiveCamera->GetProjectionMatrix();

	FFrustum Frustum;
	Frustum.ExtractFromVP(FrameRequest.SceneView.ViewMatrix * FrameRequest.SceneView.ProjectionMatrix);

	FrameRequest.SceneView.CameraPosition = FrameRequest.SceneView.ViewMatrix.GetInverse().GetTranslation();
	FrameRequest.SceneView.TotalTimeSeconds = Engine ? static_cast<float>(Engine->GetTimer().GetTotalTime()) : 0.0f;
	BuildSceneRenderPacket(Engine, ActiveWorld, Frustum, FShowFlags{}, ScenePacket);
	FrameRequest.ScenePacket = std::move(ScenePacket);

	// 실제 씬 실행과 프레임 순서는 FRenderer 내부 서브시스템이 담당한다.
	Renderer->RenderGameFrame(FrameRequest);
}
