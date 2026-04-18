#include "DefaultRenderPipeline.h"

#include "Renderer.h"
#include "Engine/Runtime/Engine.h"
#include "Component/CameraComponent.h"
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
	UCameraComponent* Camera = World ? World->GetActiveCamera() : nullptr;
	FScene* Scene = nullptr;
	if (Camera)
	{
		Frame.SetCameraInfo(Camera);

		FViewportRenderOptions Opts;
		Opts.ViewMode = EViewMode::Lit_Phong;
		Frame.SetRenderOptions(Opts);

		Scene = &World->GetScene();
		Scene->ClearFrameData();

		Builder.BeginCollect(Frame, Scene->GetProxyCount());
		Collector.CollectWorld(World, Frame, Builder);
		Collector.CollectDebugDraw(Frame, *Scene);
		Builder.BuildDynamicCommands(Frame, Scene);
	}
	else
	{
		Builder.BeginCollect(Frame);
		Builder.BuildDynamicCommands(Frame, nullptr);
	}

	Renderer.BeginFrame();
	Renderer.Render(Frame, *Scene);
	Renderer.EndFrame();
}
