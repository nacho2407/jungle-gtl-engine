#include "ObjViewer/ObjViewerRenderPipeline.h"

#include "ObjViewer/ObjViewerEngine.h"
#include "Render/Pipeline/Renderer.h"
#include "Viewport/Viewport.h"
#include "Component/CameraComponent.h"
#include "GameFramework/World.h"

FObjViewerRenderPipeline::FObjViewerRenderPipeline(UObjViewerEngine* InEngine, FRenderer& InRenderer)
	: Engine(InEngine)
{
}

FObjViewerRenderPipeline::~FObjViewerRenderPipeline()
{
}

void FObjViewerRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
	// 오프스크린 RT에 3D 씬 렌더
	RenderPreviewViewport(Renderer);

	// 스왑체인 백버퍼 → ImGui 합성 → Present
	Renderer.BeginFrame();
	Engine->RenderUI(DeltaTime);
	Renderer.EndFrame();
}

void FObjViewerRenderPipeline::RenderPreviewViewport(FRenderer& Renderer)
{
	FObjViewerViewportClient* VC = Engine->GetViewportClient();
	if (!VC) return;

	UCameraComponent* Camera = VC->GetCamera();
	if (!Camera) return;

	FViewport* VP = VC->GetViewport();
	if (!VP) return;

	ID3D11DeviceContext* Ctx = Renderer.GetFD3DDevice().GetDeviceContext();

	// 지연 리사이즈 적용 + 오프스크린 RT 바인딩
	if (VP->ApplyPendingResize())
	{
		Camera->OnResize(static_cast<int32>(VP->GetWidth()), static_cast<int32>(VP->GetHeight()));
	}
	const float ClearColor[4] = { 0.15f, 0.15f, 0.15f, 1.0f };
	VP->BeginRender(Ctx, ClearColor);

	// Bus 설정
	Bus.Clear();

	UWorld* World = Engine->GetWorld();

	Bus.SetCameraInfo(Camera);

	FShowFlags ShowFlags;
	ShowFlags.bGrid = false;
	ShowFlags.bGizmo = false;
	ShowFlags.bBillboardText = false;
	ShowFlags.bBoundingVolume = false;
	Bus.SetRenderSettings(EViewMode::Lit, ShowFlags);
	Bus.SetViewportInfo(VP);

	// 월드 수집 (선택 액터 없음)
	TArray<AActor*> EmptySelection;
	Collector.CollectWorld(World, EmptySelection, Bus);

	// GPU 렌더
	Renderer.PrepareBatchers(Bus);
	Renderer.Render(Bus);
}
