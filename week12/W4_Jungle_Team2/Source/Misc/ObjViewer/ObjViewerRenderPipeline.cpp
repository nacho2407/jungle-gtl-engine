#include "Misc/ObjViewer/ObjViewerRenderPipeline.h"
#include "Misc/ObjViewer/ObjViewerEngine.h"

#include "Render/Renderer/Renderer.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "GameFramework/World.h"
#include "Core/Logging/Stats.h"
#include "Core/Logging/GPUProfiler.h"
#include "Viewport/ViewportCamera.h"

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
	FViewportCamera* Camera = Engine->GetCamera();
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

	auto& VC = Engine->GetViewportClient();
	Renderer.GetFD3DDevice().SetSubViewport(VC.GetViewportX(),VC.GetViewportY(), VC.GetViewportWidth(), VC.GetViewportHeight());

	Renderer.Render(Bus);
	Engine->RenderUI(DeltaTime);
	Renderer.EndFrame();
}