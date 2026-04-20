#include "DefaultRenderPipeline.h"

#include "Renderer.h"
#include "Engine/Runtime/Engine.h"
#include "Editor/Viewport/ViewportCamera.h"
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
	FViewportCamera* Camera = World ? World->GetActiveCamera() : nullptr;
	if (Camera)
	{
		FMatrix ViewMat = Camera->GetViewMatrix();
		FMatrix ProjMat = Camera->GetProjectionMatrix();

        FRenderSettings RenderSettings = {};

		Bus.SetViewProjection(Camera->GetViewMatrix(), Camera->GetProjectionMatrix());
		Bus.SetRenderSettings(RenderSettings);
		Bus.SetFXAAEnabled(true);

		const FFrustum& ViewFrustum = Camera->GetFrustum();
		Collector.CollectWorld(World, RenderSettings.ShowFlags, RenderSettings.ViewMode, Bus, &ViewFrustum);
	}

	Renderer.PrepareBatchers(Bus);
	Renderer.BeginFrame();
	Renderer.Render(Bus);
	Renderer.EndFrame();
}
