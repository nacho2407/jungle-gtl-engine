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
		Bus.SetFXAAEnabled(true);

		Collector.CollectWorld(World, ShowFlags, ViewMode, Bus);
		Collector.CollectGrid(Settings.GridSpacing, Settings.GridHalfLineCount, Bus);
	}

	Renderer.PrepareBatchers(Bus);
	Renderer.BeginFrame();

	TransferViewportData(Renderer);
	Renderer.Render(Bus);
	Engine->RenderUI(DeltaTime);
	Renderer.EndFrame();
}

// 렌더러에 데이터를 전송해 뷰포트 크기에 맞는 서브 뷰포트 세팅을 요청한다.
void FObjViewerRenderPipeline::TransferViewportData(FRenderer& Renderer)
{
	auto& VC = Engine->GetViewportClient();

	int32 vx = static_cast<uint32>(VC.GetViewportX());
    int32 vy = static_cast<uint32>(VC.GetViewportY());
    int32 vw = static_cast<uint32>(VC.GetViewportWidth());
    int32 vh = static_cast<uint32>(VC.GetViewportHeight());

	Renderer.GetFD3DDevice().SetSubViewport(vx, vy, vw, vh);
}
