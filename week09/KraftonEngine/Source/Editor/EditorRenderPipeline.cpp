#include "EditorRenderPipeline.h"
#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Render/Pipeline/Renderer.h"
#include "Render/Proxy/FScene.h"
#include "Viewport/Viewport.h"
#include "Component/CameraComponent.h"
#include "GameFramework/World.h"
#include "Profiling/Stats.h"
#include "Profiling/GPUProfiler.h"

FEditorRenderPipeline::FEditorRenderPipeline(UEditorEngine* InEditor, FRenderer& InRenderer)
	: Editor(InEditor)
{
}

FEditorRenderPipeline::~FEditorRenderPipeline()
{
}

void FEditorRenderPipeline::OnSceneCleared()
{
	GPUOcclusion.InvalidateResults();
}

void FEditorRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
#if STATS
	FStatManager::Get().TakeSnapshot();
	FGPUProfiler::Get().TakeSnapshot();
	FGPUProfiler::Get().BeginFrame();
#endif

	for (FLevelEditorViewportClient* ViewportClient : Editor->GetLevelViewportClients())
	{
		SCOPE_STAT_CAT("RenderViewport", "2_Render");
		RenderViewport(ViewportClient, Renderer);
	}

	// 스왑체인 백버퍼 복귀 → ImGui 합성 → Present
	Renderer.BeginFrame();
	{
		SCOPE_STAT_CAT("EditorUI", "5_UI");
		Editor->RenderUI(DeltaTime);
	}

#if STATS
	FGPUProfiler::Get().EndFrame();
#endif

	{
		SCOPE_STAT_CAT("Present", "2_Render");
		Renderer.EndFrame();
	}
}

void FEditorRenderPipeline::RenderViewport(FLevelEditorViewportClient* VC, FRenderer& Renderer)
{
	UCameraComponent* Camera = VC->GetCamera();
	if (!Camera) return;

	FViewport* VP = VC->GetViewport();
	if (!VP) return;

	ID3D11DeviceContext* Ctx = Renderer.GetFD3DDevice().GetDeviceContext();
	if (!Ctx) return;

	UWorld* World = Editor->GetWorld();
	if (!World) return;

	// GPU Occlusion 지연 초기화
	if (!GPUOcclusion.IsInitialized())
		GPUOcclusion.Initialize(Renderer.GetFD3DDevice().GetDevice());

	// 이전 프레임 Occlusion 결과 읽기 (staging → OccludedSet)
	GPUOcclusion.ReadbackResults(Ctx);

	// 뷰포트별 렌더 옵션 사용
	const FViewportRenderOptions& Opts = VC->GetRenderOptions();
	const FShowFlags& ShowFlags = Opts.ShowFlags;
	EViewMode ViewMode = Opts.ViewMode;

	// 지연 리사이즈 적용 + 오프스크린 RT 바인딩
	if (VP->ApplyPendingResize())
	{
		Camera->OnResize(static_cast<int32>(VP->GetWidth()), static_cast<int32>(VP->GetHeight()));
	}

	// 렌더 시작 (RT 클리어 + DSV 바인딩)
	VP->BeginRender(Ctx);

	// 1. Frame 설정
	Frame.ClearViewportResources();
	FScene& Scene = World->GetScene();
	Scene.ClearFrameData();

	Frame.SetCameraInfo(Camera);
	Frame.SetRenderSettings(ViewMode, ShowFlags);
	Frame.SetViewportInfo(VP);
	Frame.ViewportType = Opts.ViewportType;
	Frame.OcclusionCulling = &GPUOcclusion;
	Frame.LODContext = World->PrepareLODContext();

	// 2. BeginCollect → Proxy → FDrawCommand 직접 변환
	Renderer.BeginCollect(Frame, Scene.GetProxyCount());

	{
		SCOPE_STAT_CAT("Collector", "3_Collect");

		Collector.CollectWorld(World, Frame, Renderer);

		Collector.CollectGrid(Opts.GridSpacing, Opts.GridHalfLineCount, Scene);
		Collector.CollectDebugDraw(World->GetDebugDrawQueue(), Frame, Scene);

		if (ShowFlags.bOctree)
			Collector.CollectOctreeDebug(World->GetOctree(), Scene);

		if (VC == Editor->GetActiveViewport())
			Collector.CollectOverlayText(Editor->GetOverlayStatSystem(), *Editor, Scene);

		Renderer.BuildDynamicCommands(Frame, &Scene);
	}

	// 3. GPU 정렬 + 제출 (PostProcess 패스에서 Fog/Outline도 자동 처리)
	{
		SCOPE_STAT_CAT("Renderer.Render", "4_ExecutePass");
		Renderer.Render(Frame);
	}

	// 4. GPU Occlusion — DSV 언바인딩 후 Hi-Z 생성 + Occlusion Test 디스패치
	if (GPUOcclusion.IsInitialized())
	{
		SCOPE_STAT_CAT("GPUOcclusion", "4_ExecutePass");

		// DSV 언바인딩 (DepthSRV 읽기와 동시 바인딩 불가)
		ID3D11RenderTargetView* rtv = VP->GetRTV();
		Ctx->OMSetRenderTargets(1, &rtv, nullptr);

		GPUOcclusion.DispatchOcclusionTest(
			Ctx,
			VP->GetDepthSRV(),
			Collector.GetLastVisibleProxies(),
			Frame.View, Frame.Proj,
			VP->GetWidth(), VP->GetHeight());
	}
}
