#include "DefaultRenderPipeline.h"

#include "Renderer.h"
#include "Engine/Runtime/Engine.h"
#include "Render/Types/MinimalViewInfo.h"
#include "GameFramework/World.h"

FDefaultRenderPipeline::FDefaultRenderPipeline(UEngine* InEngine, FRenderer& InRenderer)
	: Engine(InEngine)
{
}

FDefaultRenderPipeline::~FDefaultRenderPipeline()
{
}

void FDefaultRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
	Frame.ClearViewportResources();

	FDrawCommandBuilder& Builder = Renderer.GetBuilder();

	UWorld* World = Engine->GetWorld();
	FMinimalViewInfo POV;
	const bool bHasPOV = World && World->GetActivePOV(POV);
	FScene* Scene = nullptr;
	if (bHasPOV)
	{
		Frame.SetCameraInfo(POV);

		Frame.WorldType = World->GetWorldType();

		FViewportRenderOptions Opts;
		Opts.ViewMode = EViewMode::Lit_Phong;
		Frame.SetRenderOptions(Opts);

		Scene = &World->GetScene();
		Scene->ClearFrameData();

		Builder.BeginCollect(Frame);
		FCollectOutput Output;
		Collector.Collect(World, Frame, Output);
		Collector.CollectDebugDraw(Frame, *Scene);
		Builder.BuildCommands(Frame, Scene, Output);
	}
	else
	{
		Builder.BeginCollect(Frame);
		FCollectOutput EmptyOutput;
		Builder.BuildCommands(Frame, nullptr, EmptyOutput);
	}

	Renderer.BeginFrame();
	Renderer.Render(Frame, *Scene);
	Renderer.EndFrame();
}
