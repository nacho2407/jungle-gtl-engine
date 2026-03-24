#include "DefaultRenderPipeline.h"

#include "Renderer.h"
#include "Engine/Runtime/Engine.h"
#include "Render/Scene/RenderCollectorContext.h"
#include "Component/CameraComponent.h"
#include "GameFramework/World.h"

FDefaultRenderPipeline::FDefaultRenderPipeline(UEngine* InEngine, FRenderer& InRenderer)
	: Engine(InEngine)
{
	Collector.Initialize(InRenderer.GetFD3DDevice().GetDevice());
}

FDefaultRenderPipeline::~FDefaultRenderPipeline()
{
	Collector.Release();
}

void FDefaultRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
	Bus.Clear();

	UWorld* World = Engine->GetWorld();
	UCameraComponent* Camera = World ? World->GetActiveCamera() : nullptr;
	if (Camera)
	{
		FRenderCollectorContext Context;
		Context.Camera = Camera;

		TArray<FCollectPhase> Phases;
		Phases.push_back([&](FRenderBus& B) {
			Collector.CollectWorld(World, Context.ShowFlags, Context.ViewMode, B);
		});

		Collector.Collect(Context, Phases, Bus);
	}

	Renderer.BeginFrame();
	Renderer.Render(Bus);
	Renderer.EndFrame();
}
