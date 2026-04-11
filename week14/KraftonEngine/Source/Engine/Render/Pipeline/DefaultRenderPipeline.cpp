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

	UWorld* World = Engine->GetWorld();
	UCameraComponent* Camera = World ? World->GetActiveCamera() : nullptr;
	FScene* Scene = nullptr;
	if (Camera)
	{
		FShowFlags ShowFlags;
		EViewMode ViewMode = EViewMode::Lit;

		Frame.SetCameraInfo(Camera);
		Frame.SetRenderSettings(ViewMode, ShowFlags);

		Scene = &World->GetScene();
		Scene->ClearFrameData();

		Renderer.BeginCollect(Frame, Scene->GetProxyCount());
		Collector.CollectWorld(World, Frame, Renderer);
		Collector.CollectDebugDraw(World->GetDebugDrawQueue(), Frame, *Scene);
	}
	else
	{
		Renderer.BeginCollect(Frame);
	}

	Renderer.BeginFrame();
	Renderer.Render(Frame, Scene);
	Renderer.EndFrame();
}
