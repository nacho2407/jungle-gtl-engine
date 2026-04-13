#include "EditorRenderPipeline.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/ViewportCamera.h"
#include "Render/Renderer/Renderer.h"
#include "Component/GizmoComponent.h"
#include "GameFramework/World.h"
#include "Core/Logging/Stats.h"
#include "Core/Logging/GPUProfiler.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Runtime/SceneView.h"

FEditorRenderPipeline::FEditorRenderPipeline(UEditorEngine* InEditor, FRenderer& InRenderer) : Editor(InEditor)
{
    Collector.Initialize(InRenderer.GetFD3DDevice().GetDevice());
    ViewportCullingStats.resize(FViewportLayout::MaxViewports);
	ViewportDecalStats.resize(FViewportLayout::MaxViewports);
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

    if (!Editor->GetWorld())
        return;

    // 화면 Resize 목적으로 render 전에 화면 resizing
    const FViewportRect& HostRect = Editor->GetViewportLayout().GetHostRect();
    int32                TargetWidth = HostRect.Width;
    int32                TargetHeight = HostRect.Height;

    if ((TargetWidth <= 0 || TargetHeight <= 0) && Editor->GetWindow())
    {
        TargetWidth = static_cast<int32>(Editor->GetWindow()->GetWidth());
        TargetHeight = static_cast<int32>(Editor->GetWindow()->GetHeight());
    }

    if (TargetWidth > 0 && TargetHeight > 0)
    {
        Renderer.GetFD3DDevice().EnsureViewportRenderTargets(TargetWidth, TargetHeight);
    }
    else
    {
        Renderer.InvalidateSceneFinalTargets();
    }

    // 1회: 전체 백버퍼 클리어 (색상 + 깊이/스텐실)
    Renderer.BeginFrame();
    Renderer.UseViewportRenderTargets();

    // 4개 뷰포트를 순서대로 렌더링
    for (int32 i = 0; i < FViewportLayout::MaxViewports; ++i)
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
    const FViewportRect& HostRect = Editor->GetViewportLayout().GetHostRect();
    const int32          LocalX = Rect.X - HostRect.X;
    const int32          LocalY = Rect.Y - HostRect.Y;

    Renderer.GetFD3DDevice().SetSubViewport(LocalX, LocalY, Rect.Width, Rect.Height);

    // 3. 이 뷰포트용 렌더 데이터 수집
    Bus.Clear();

    UWorld*                World = Editor->GetWorld();
    const FEditorSettings& Settings = Editor->GetSettings();
    const FShowFlags&      ShowFlags = Settings.ShowFlags;
    const EViewMode        ViewMode = SceneView.ViewMode;

    Bus.SetViewProjection(Camera->GetViewMatrix(), Camera->GetProjectionMatrix());
    Bus.SetRenderSettings(ViewMode, ShowFlags);
    Bus.SetFXAAThreshold(Settings.FXAAThreshold);
	Bus.SetViewportSize(FVector2(Rect.Width, Rect.Height));

    const FFrustum& ViewFrustum = Camera->GetFrustum();
    Collector.CollectWorld(World, ShowFlags, ViewMode, Bus, &ViewFrustum);
    ViewportCullingStats[ViewportIndex] = Collector.GetLastCullingStats();
    ViewportDecalStats[ViewportIndex] = Collector.GetLastDecalStats();
    Collector.CollectGrid(Settings.GridSpacing, Settings.GridHalfLineCount, Bus, Camera->IsOrthographic());

    // 뷰포트별 카메라 기준으로 기즈모 스케일 결정
    // TickInteraction 에서 한 번만 처리하면 마지막 뷰포트가 다른 뷰포트의 스케일을 덮어쓰므로
    // CollectGizmo 직전에 각 뷰포트 카메라로 적용합니다.
    if (Editor->GetEditorState() == EEditorState::Edit)
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
