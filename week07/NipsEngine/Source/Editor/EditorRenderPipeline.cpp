#include "EditorRenderPipeline.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/ViewportCamera.h"
#include "Render/Renderer/Renderer.h"
#include "GameFramework/World.h"
#include "Core/Logging/Stats.h"
#include "Core/Logging/GPUProfiler.h"
#include "Runtime/SceneView.h"
#include "Engine/Component/GizmoComponent.h"

FEditorRenderPipeline::FEditorRenderPipeline(UEditorEngine* InEditor, FRenderer& InRenderer) : Editor(InEditor)
{
    Collector.Initialize(InRenderer.GetFD3DDevice().GetDevice());
    ViewportCullingStats.resize(FEditorViewportLayout::MaxViewports);
	ViewportDecalStats.resize(FEditorViewportLayout::MaxViewports);
}

FEditorRenderPipeline::~FEditorRenderPipeline() { Collector.Release(); }

void FEditorRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
#if STATS
    FStatManager::Get().TakeSnapshot();
    FGPUProfiler::Get().TakeSnapshot();
#endif

    for (FRenderCollector::FCullingStats& Stats : ViewportCullingStats)
    {
        Stats = {};
    }

    if (!Editor->GetFocusedWorld())
        return;

    // 1회: 전체 백버퍼 클리어 (색상 + 깊이/스텐실)
    Renderer.BeginFrame();

    // 4개 뷰포트를 순서대로 렌더링
    for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
    {
        RenderViewport(Renderer, i);
    }

    Renderer.UseBackBufferRenderTargets();

    // ImGui UI 오버레이
    Editor->RenderUI(DeltaTime);

    Renderer.EndFrame();
}

void FEditorRenderPipeline::RenderViewport(FRenderer& Renderer, int32 ViewportIndex)
{
    FEditorViewportClient& VC = Editor->GetViewportLayout().GetViewportClient(ViewportIndex);

    FViewportCamera* Camera = VC.GetCamera();
    if (!Camera)
        return;

    // 1. 이 뷰포트의 SceneView 빌드
    //    - ViewRect : 화면 내 서브 영역 (BuildSceneView가 State->Rect에서 채움)
    //    - ViewMode : 뷰포트별 독립 모드 (기본값 EViewMode::Lit)
    FSceneView SceneView;
    VC.BuildSceneView(SceneView);

    // 2. 렌더링 대상을 서브 영역으로 제한
    const FViewportRect& Rect = SceneView.ViewRect;
    if (Rect.Width <= 0 || Rect.Height <= 0)
        return;

    FSceneViewport& SceneViewport = Editor->GetViewportLayout().GetSceneViewport(ViewportIndex);
    
	// Width, Height 변경 여부에 따라 Resource 버퍼 재생성
	// 만약 최소화 등의 상황으로 (H, W) == (0, 0) 일 경우 Render 안함
	if (!SceneViewport.EnsureResource(
            Renderer.GetFD3DDevice().GetDevice(),
            static_cast<uint32>(Rect.Width),
            static_cast<uint32>(Rect.Height)))
    {
        return;
    }

    // Viewport 별 버퍼 클리어 및 Renderer 버퍼 세팅
    Renderer.BeginViewportFrame(SceneViewport.GetViewportRenderTargets());

    // 3. 이 뷰포트용 렌더 데이터 수집
    Bus.Clear();

    // 각 뷰포트는 자신이 참조하는 월드를 렌더링합니다.
    UWorld*                World = VC.GetFocusedWorld();
    const FEditorSettings& Settings = Editor->GetSettings();
    const FShowFlags&      ShowFlags = Settings.ShowFlags;
    const EViewMode        ViewMode = SceneView.ViewMode;

    Bus.SetViewProjection(Camera->GetViewMatrix(), Camera->GetProjectionMatrix());
    Bus.SetRenderSettings(ViewMode, ShowFlags);
	Bus.SetViewportSize(FVector2(static_cast<float>(Rect.Width), static_cast<float>(Rect.Height)));
    Bus.SetViewportOrigin(FVector2(0.0f, 0.0f));
    Bus.SetFXAAEnabled(Settings.bEnableFXAA && !Camera->IsOrthographic());

    const FFrustum& ViewFrustum = Camera->GetFrustum();
    Collector.CollectWorld(World, ShowFlags, ViewMode, Bus, &ViewFrustum);
    ViewportCullingStats[ViewportIndex] = Collector.GetLastCullingStats();
    ViewportDecalStats[ViewportIndex] = Collector.GetLastDecalStats();
    Collector.CollectGrid(Settings.GridSpacing, Settings.GridHalfLineCount, Bus, Camera->IsOrthographic());

    // 이 뷰포트가 편집 모드일 때만 기즈모·선택 오버레이를 그립니다.
    if (VC.GetPlayState() == EViewportPlayState::Editing)
    {
        if (UGizmoComponent* Gizmo = Editor->GetGizmo())
        {
            if (Camera->IsOrthographic())
                Gizmo->ApplyScreenSpaceScalingOrtho(Camera->GetOrthoHeight());
            else
                Gizmo->ApplyScreenSpaceScaling(SceneView.CameraPosition);
        }

        Collector.CollectGizmo(Editor->GetGizmo(), ShowFlags, Bus, VC.GetViewportState()->bHovered);
        Collector.CollectSelection(Editor->GetSelectionManager().GetSelectedActors(), ShowFlags, ViewMode, Bus);
    }

    // 4. CPU 배처 데이터 준비 → GPU 드로우 (SetSubViewport 영역에만 출력됨)
    Renderer.PrepareBatchers(Bus);
    Renderer.Render(Bus);
}

const FRenderCollector::FCullingStats& FEditorRenderPipeline::GetViewportCullingStats(int32 ViewportIndex) const
{
    static const FRenderCollector::FCullingStats EmptyStats{};

    if (ViewportIndex < 0 || ViewportIndex >= static_cast<int32>(ViewportCullingStats.size()))
    {
        return EmptyStats;
    }

    return ViewportCullingStats[ViewportIndex];
}

const FRenderCollector::FDecalStats& FEditorRenderPipeline::GetViewportDecalStats(int32 ViewportIndex) const
{
	static const FRenderCollector::FDecalStats EmptyStats{};

	if (ViewportIndex < 0 || ViewportIndex >= static_cast<int32>(ViewportCullingStats.size()))
	{
		return EmptyStats;
	}

	return ViewportDecalStats[ViewportIndex];
}
