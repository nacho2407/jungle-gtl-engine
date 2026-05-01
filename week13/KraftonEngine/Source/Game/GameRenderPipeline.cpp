#include "Game/GameRenderPipeline.h"

#include "Game/GameEngine.h"
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
	Frame.ClearViewportResources();

	FDrawCommandBuilder& Builder = Renderer.GetBuilder();

	UWorld* World = Game->GetWorld();
	UCameraComponent* Camera = World ? World->GetActiveCamera() : nullptr;
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

	if (VP->ApplyPendingResize())
	{
		Camera->OnResize(static_cast<int32>(VP->GetWidth()), static_cast<int32>(VP->GetHeight()));
	}

	ID3D11DeviceContext* Ctx = Renderer.GetFD3DDevice().GetDeviceContext();

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
}

void FGameRenderPipeline::CollectCommands(FScene* Scene, FRenderer& Renderer, FCollectOutput& Output)
{
	FDrawCommandBuilder& Builder = Renderer.GetBuilder();
	Builder.BeginCollect(Frame, Scene->GetProxyCount());

	Collector.Collect(Game->GetWorld(), Frame, Output);
	Builder.BuildCommands(Frame, Scene, Output);
}
