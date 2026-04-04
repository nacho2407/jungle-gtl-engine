#include "EditorRenderPipeline.h"
#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Render/Pipeline/Renderer.h"
#include "Viewport/Viewport.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
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

void FEditorRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
#if STATS
	FStatManager::Get().TakeSnapshot();
	FGPUProfiler::Get().TakeSnapshot();
	FGPUProfiler::Get().BeginFrame();
#endif

	{
		SCOPE_STAT_CAT("RenderViewport", "Viewport");
		for (FLevelEditorViewportClient* VC : Editor->GetLevelViewportClients())
		{
			RenderViewport(VC, Renderer);
		}
	}
	// 뷰포트별 오프스크린 렌더 (각 VP의 RT에 3D 씬 렌더)

	// 스왑체인 백버퍼 복귀 → ImGui 합성 → Present
	Renderer.BeginFrame();
	{
		SCOPE_STAT_CAT("EditorUI", "UI(ImGui)");
		Editor->RenderUI(DeltaTime);
	}

#if STATS
	FGPUProfiler::Get().EndFrame();
#endif

	{
		SCOPE_STAT_CAT("Present", "GPU");
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

	// 1. Bus 수집
	Bus.Clear();

	Bus.SetCameraInfo(Camera);
	Bus.SetRenderSettings(ViewMode, ShowFlags);
	Bus.SetViewportInfo(VP);
	Bus.SetViewportType(Opts.ViewportType);

	// 2. 프록시 + Batcher Entry를 ERenderPass별로 수집
	{
		SCOPE_STAT("Collector");
		Collector.CollectWorld(World, Bus);

		if (UGizmoComponent* Gizmo = Editor->GetGizmo())
			Gizmo->UpdateAxisMask(Opts.ViewportType);

		Collector.CollectGrid(Opts.GridSpacing, Opts.GridHalfLineCount, Bus);
		Collector.CollectDebugDraw(World->GetDebugDrawQueue(), Bus);

		if (ShowFlags.bOctree)
			Collector.CollectOctreeDebug(World->GetOctree(), Bus);

		if (VC == Editor->GetActiveViewport())
			Collector.CollectOverlayText(Editor->GetOverlayStatSystem(), *Editor, Bus);
	}

	// 3. Batcher 준비
	{
		SCOPE_STAT("PrepareBatcher");
		Renderer.PrepareBatchers(Bus);
	}

	// 4. GPU 드로우 콜 실행
	{
		SCOPE_STAT("Renderer.Render");
		Renderer.Render(Bus);
	}
}
