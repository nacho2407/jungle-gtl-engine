#include "Editor/ObjViewerRenderPipeline.h"
#include "Editor/ObjViewerEngine.h"

#include "Render/Renderer/Renderer.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "GameFramework/World.h"
#include "Core/Stats.h"
#include "Core/GPUProfiler.h"

FObjViewerRenderPipeline::FObjViewerRenderPipeline(UObjViewerEngine* InEngine, FRenderer& InRenderer)
	: Engine(InEngine)
{
	Collector.Initialize(InRenderer.GetFD3DDevice().GetDevice());
}

FObjViewerRenderPipeline::~FObjViewerRenderPipeline()
{
	Collector.Release();
}

void FObjViewerRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
	Bus.Clear();

	UWorld* World = Engine->GetWorld();
	UCameraComponent* Camera = World ? World->GetActiveCamera() : nullptr;
	if (Camera)
	{
		const auto& Settings = Engine->GetSettings();
		const FShowFlags& ShowFlags = Settings.ShowFlags;
		EViewMode ViewMode = Settings.ViewMode;

		Bus.SetViewProjection(Camera->GetViewMatrix(), Camera->GetProjectionMatrix());
		Bus.SetRenderSettings(ViewMode, ShowFlags);

		Collector.CollectWorld(World, ShowFlags, ViewMode, Bus);
		Collector.CollectGrid(Settings.GridSpacing, Settings.GridHalfLineCount, Bus);
	}

	Renderer.PrepareBatchers(Bus);
	Renderer.BeginFrame();
	Renderer.Render(Bus);
	Engine->RenderUI(DeltaTime);
	Renderer.EndFrame();
}
