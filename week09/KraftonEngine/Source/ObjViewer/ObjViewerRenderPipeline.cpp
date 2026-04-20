//#include "ObjViewer/ObjViewerRenderPipeline.h"
//
//#include "ObjViewer/ObjViewerEngine.h"
//#include "Render/Renderer/Renderer.h"
//#include "Render/Scene/FScene.h"
//#include "Viewport/Viewport.h"
//#include "Component/CameraComponent.h"
//#include "GameFramework/World.h"
//#include "Render/Core/RenderPipeline.h"
//
//FObjViewerRenderPipeline::FObjViewerRenderPipeline(UObjViewerEngine* InEngine, FRenderer& InRenderer)
//	: Engine(InEngine)
//{
//}
//
//FObjViewerRenderPipeline::~FObjViewerRenderPipeline()
//{
//}
//
//void FObjViewerRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
//{
//	// 오프스크린 RT에 3D 씬 렌더
//	RenderPreviewViewport(Renderer);
//
//	// 스왑체인 백버퍼 → ImGui 합성 → Present
//	Renderer.BeginFrame();
//	Engine->RenderUI(DeltaTime);
//	Renderer.EndFrame();
//}
//
//void FObjViewerRenderPipeline::RenderPreviewViewport(FRenderer& Renderer)
//{
//	FObjViewerViewportClient* VC = Engine->GetViewportClient();
//	if (!VC) return;
//
//	UCameraComponent* Camera = VC->GetCamera();
//	if (!Camera) return;
//
//	FViewport* VP = VC->GetViewport();
//	if (!VP) return;
//
//	ID3D11DeviceContext* Ctx = Renderer.GetFD3DDevice().GetDeviceContext();
//
//	// 지연 리사이즈 적용 + 오프스크린 RT 바인딩
//	if (VP->ApplyPendingResize())
//	{
//		Camera->OnResize(static_cast<int32>(VP->GetWidth()), static_cast<int32>(VP->GetHeight()));
//	}
//	const float ClearColor[4] = { 0.15f, 0.15f, 0.15f, 1.0f };
//	VP->BeginRender(Ctx, ClearColor);
//
//	// Frame 설정
//	Frame.ClearViewportResources();
//
//	UWorld* World = Engine->GetWorld();
//	FScene& Scene = World->GetScene();
//	Scene.ClearFrameData();
//
//	Frame.SetCameraInfo(Camera);
//	Frame.SetViewportInfo(VP);
//
//	FShowFlags ShowFlags;
//	ShowFlags.bGrid = false;
//	ShowFlags.bGizmo = false;
//	ShowFlags.bBillboardText = false;
//	ShowFlags.bBoundingVolume = false;
//	Frame.SetRenderSettings(EViewMode::Lit_Phong, ShowFlags);
//
//	if (const auto* Registry = Renderer.GetViewModePassRegistry();
//		Registry && Registry->HasConfig(EViewMode::Lit_Phong))
//	{
//		Renderer.SetActiveViewMode(EViewMode::Lit_Phong);
//		Renderer.AcquireViewModeSurfaceSet(VP->GetWidth(), VP->GetHeight());
//	}
//	else
//	{
//		Renderer.ClearActiveViewMode();
//		Renderer.ReleaseViewModeSurfaceSet();
//	}
//
//	// BeginCollect → 월드 수집 → 동적 커맨드 → Scene pipeline 실행
//	Renderer.BeginCollect(Frame, Scene.GetPrimitiveProxyCount());
//	Collector.CollectWorld(World, Frame, Renderer);
//	Renderer.BuildDynamicCommands(Frame, &Scene);
//	Renderer.PreparePipelineExecution(Frame);
//	Renderer.ExecutePipeline(ERenderPipelineType::Scene, Frame);
//	Renderer.FinalizePipelineExecution();
//}
