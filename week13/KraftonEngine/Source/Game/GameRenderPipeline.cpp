#include "Game/GameRenderPipeline.h"

#include "Game/GameEngine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerCameraManager.h"
#include "Component/CameraComponent.h"
#include "Input/InputSystem.h"
#include "Viewport/Viewport.h"

FGameRenderPipeline::FGameRenderPipeline(UGameEngine* InGame, FRenderer& InRenderer)
	: Game(InGame)
{
}

FGameRenderPipeline::~FGameRenderPipeline()
{
}

void FGameRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
	ID3D11DeviceContext* Ctx = Renderer.GetFD3DDevice().GetDeviceContext();
	if (!Ctx) return;

	Frame.ClearViewportResources();

	FDrawCommandBuilder& Builder = Renderer.GetBuilder();

	UWorld* World = Game->GetWorld();
	// 잔여 정리: PrepareViewport 의 OnResize 호출이 컴포넌트 의존 — PC->PlayerCameraManager
	// 의 ActiveCamera 를 그대로 사용. 향후 PrepareViewport 시그니처가 NotifyResize 패턴으로
	// 정리되면 이 경로도 사라진다.
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APlayerCameraManager* CM = PC ? PC->GetPlayerCameraManager() : nullptr;
	UCameraComponent* Camera = CM ? CM->GetActiveCamera() : nullptr;
	FViewport* VP = Game->GetStandaloneViewport();
	if (!World || !Camera || !VP)
	{
		Renderer.BeginFrame();
		Renderer.EndFrame();
		return;
	}

	Frame.WorldType = World->GetWorldType();

	FViewportRenderOptions Opts;
	Opts.ViewMode = EViewMode::Lit_Phong;
	Frame.SetRenderOptions(Opts);

	FScene* Scene = &World->GetScene();

	PrepareViewport(VP, Camera, Ctx);
	BuildFrame(VP, Camera, Scene);

	FCollectOutput Output;
	CollectCommands(Scene, Renderer, Output);

	Renderer.Render(Frame, *Scene);

	Renderer.BeginFrame();
	Renderer.BlitToBackBuffer(VP->GetSRV());
	Renderer.EndFrame();
}

void FGameRenderPipeline::PrepareViewport(FViewport* VP, UCameraComponent* Camera, ID3D11DeviceContext* Ctx)
{
	if (VP->ApplyPendingResize())
	{
		Camera->OnResize(static_cast<int32>(VP->GetWidth()), static_cast<int32>(VP->GetHeight()));
	}
	VP->BeginRender(Ctx);
}

void FGameRenderPipeline::BuildFrame(FViewport* VP, UCameraComponent* Camera, FScene* Scene)
{
	Frame.ClearViewportResources();
	Frame.SetCameraInfo(Camera);
	Frame.SetViewportInfo(VP);

	const POINT MousePos = InputSystem::Get().GetMouseClientPos();
	if (MousePos.x >= 0 && MousePos.y >= 0
		&& MousePos.x < static_cast<LONG>(Frame.ViewportWidth)
		&& MousePos.y < static_cast<LONG>(Frame.ViewportHeight))
	{
		Frame.CursorViewportX = static_cast<uint32>(MousePos.x);
		Frame.CursorViewportY = static_cast<uint32>(MousePos.y);
	}
	else
	{
		Frame.CursorViewportX = UINT32_MAX;
		Frame.CursorViewportY = UINT32_MAX;
	}
}

void FGameRenderPipeline::CollectCommands(FScene* Scene, FRenderer& Renderer, FCollectOutput& Output)
{
	FDrawCommandBuilder& Builder = Renderer.GetBuilder();
	Builder.BeginCollect(Frame, Scene->GetProxyCount());

	Collector.Collect(Game->GetWorld(), Frame, Output);
	Builder.BuildCommands(Frame, Scene, Output);
}
