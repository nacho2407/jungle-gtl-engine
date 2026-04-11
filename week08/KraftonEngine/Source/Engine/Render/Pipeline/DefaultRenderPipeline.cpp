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
	Bus.Clear();

	UWorld* World = Engine->GetWorld();
	UCameraComponent* Camera = World ? World->GetActiveCamera() : nullptr;
	if (Camera)
	{
		FShowFlags ShowFlags;
		EViewMode ViewMode = EViewMode::Lit;

		Bus.Frame.SetCameraInfo(Camera);
		Bus.Frame.SetRenderSettings(ViewMode, ShowFlags);

		Renderer.BeginCollect(Bus);
		Collector.CollectWorld(World, Bus, Renderer);
		Collector.CollectDebugDraw(World->GetDebugDrawQueue(), Bus);
	}
	else
	{
		Renderer.BeginCollect(Bus);
	}

	Renderer.BeginFrame();
	Renderer.Render(Bus);
	Renderer.EndFrame();
}
