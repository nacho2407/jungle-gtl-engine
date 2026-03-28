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

	// TODO-VIEWER: Slate 구조를 개편한 뒤 코드 수정하기 (주입된 정보를 바탕으로 화면 조정)
	D3D11_VIEWPORT D3DViewport = GetD3DViewport();
	ID3D11DeviceContext* Context = Renderer.GetFD3DDevice().GetDeviceContext();
    Context->RSSetViewports(1, &D3DViewport);

	Renderer.Render(Bus);
	Engine->RenderUI(DeltaTime);
	Renderer.EndFrame();
}

// TODO-VIEWER: Slate 구조를 개편한 뒤 코드 수정하기
D3D11_VIEWPORT FObjViewerRenderPipeline::GetD3DViewport() const
{
    auto& VC = Engine->GetViewportClient();
    D3D11_VIEWPORT D3DViewport;
    D3DViewport.TopLeftX = VC.GetViewportX();
    D3DViewport.TopLeftY = VC.GetViewportY();
    D3DViewport.Width    = VC.GetViewportWidth();
    D3DViewport.Height   = VC.GetViewportHeight();
    D3DViewport.MinDepth = 0.0f;
    D3DViewport.MaxDepth = 1.0f;
    
    return D3DViewport;
}