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

		Bus.SetViewProjection(Camera->GetViewMatrix(), Camera->GetProjectionMatrix(),
			Camera->GetForwardVector(), Camera->GetRightVector(), Camera->GetUpVector());
		Bus.SetRenderSettings(ViewMode, ShowFlags);

		Collector.CollectWorld(World, ShowFlags, ViewMode, Bus);
	}

	Renderer.PrepareBatchers(Bus);
	Renderer.BeginFrame();
	Renderer.Render(Bus);
	Renderer.EndFrame();
}
