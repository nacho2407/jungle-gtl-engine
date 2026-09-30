// ObjViewer 영역의 비활성화된 예제 구현입니다.
// #include "ObjViewer/ObjViewerRenderPipeline.h"
//
// #include "ObjViewer/ObjViewerEngine.h"
// #include "Render/Renderer/Renderer.h"
// #include "Render/Scene/FScene.h"
// #include "Viewport/Viewport.h"
// #include "Component/CameraComponent.h"
// #include "GameFramework/World.h"
// #include "Render/Core/RenderPipeline.h"
//
// FObjViewerRenderPipeline::FObjViewerRenderPipeline(UObjViewerEngine* InEngine, FRenderer& InRenderer)
//	: Engine(InEngine)
//{
// }
//
// FObjViewerRenderPipeline::~FObjViewerRenderPipeline()
//{
// }
//
// void FObjViewerRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
//{
//	// 백버퍼 RT에 3D 장면을 먼저 렌더합니다.
//	RenderPreviewViewport(Renderer);
//
//	// 프레임을 시작한 뒤 ImGui를 올리고 Present합니다.
//	Renderer.BeginFrame();
//	Engine->RenderUI(DeltaTime);
//	Renderer.EndFrame();
// }
//
// void FObjViewerRenderPipeline::RenderPreviewViewport(FRenderer& Renderer)
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
//	// 리사이즈 요청을 반영하고 백버퍼 RT를 준비합니다.
//	if (VP->ApplyPendingResize())
//	{
//		Camera->OnResize(static_cast<int32>(VP->GetWidth()), static_cast<int32>(VP->GetHeight()));
//	}
//	const float ClearColor[4] = { 0.15f, 0.15f, 0.15f, 1.0f };
//	VP->BeginRender(Ctx, ClearColor);
//
//	// Frame 데이터를 초기화합니다.
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
//	ShowFlags.bUUIDText = false;
//	ShowFlags.bBoundingVolume = false;
//	Frame.SetRenderSettings(EViewMode::Lit_Phong, ShowFlags);
//
//	if (const auto* Registry = Renderer.GetViewModePassRegistry();
//		Registry && Registry->HasConfig(EViewMode::Lit_Phong))
//	{
//		Renderer.SetActiveViewMode(EViewMode::Lit_Phong);
//		Renderer.AcquireViewModeTextures(VP->GetWidth(), VP->GetHeight());
//	}
//	else
//	{
//		Renderer.ClearActiveViewMode();
//		Renderer.ReleaseViewModeTextures();
//	}
//
//	// BeginCollect 이후 렌더 데이터 수집과 커맨드 생성, Scene pipeline 실행을 진행합니다.
//	Renderer.BeginCollect(Frame, Scene.GetPrimitiveProxyCount());
//	Collector.CollectWorld(World, Frame, Renderer);
//	Renderer.BuildDynamicCommands(Frame, &Scene);
//	Renderer.PreparePipelineExecution(Frame);
//	Renderer.ExecutePipeline(ERenderPipelineType::ScenePipeline, Frame);
//	Renderer.FinalizePipelineExecution();
// }
