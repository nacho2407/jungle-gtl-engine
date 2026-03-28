#include "EditorRenderPipeline.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/ViewportCamera.h"
#include "Render/Renderer/Renderer.h"
#include "Component/GizmoComponent.h"
#include "GameFramework/World.h"
#include "Core/Stats.h"
#include "Core/GPUProfiler.h"
#include "Runtime/SceneView.h"

FEditorRenderPipeline::FEditorRenderPipeline(UEditorEngine* InEditor, FRenderer& InRenderer)
	: Editor(InEditor)
{
	Collector.Initialize(InRenderer.GetFD3DDevice().GetDevice());
}

FEditorRenderPipeline::~FEditorRenderPipeline()
{
	Collector.Release();
}

void FEditorRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
#if STATS
	FStatManager::Get().TakeSnapshot();
	FGPUProfiler::Get().TakeSnapshot();
#endif

	if (!Editor->GetWorld()) return;

	// 1회: 전체 백버퍼 클리어 (색상 + 깊이/스텐실)
	// BeginFrame 내부에서 RSSetViewports(전체창)도 호출되므로
	// 각 뷰포트에서 SetSubViewport로 덮어씁니다.
	Renderer.BeginFrame();

	// 4개 뷰포트를 순서대로 렌더링
	for (int32 i = 0; i < UEditorEngine::MaxViewports; ++i)
	{
		RenderViewport(Renderer, i);
	}

	// ImGui UI 오버레이 + SwapChain Present
	Editor->RenderUI(DeltaTime);
	Renderer.EndFrame();
}

void FEditorRenderPipeline::RenderViewport(FRenderer& Renderer, int32 ViewportIndex)
{
	FEditorViewportClient& VC = Editor->GetViewportClient(ViewportIndex);

	UCameraComponent* Camera = VC.GetCamera();
	if (!Camera) return;

	// 1. 이 뷰포트의 SceneView 빌드
	//    - ViewRect : 화면 내 서브 영역 (BuildSceneView가 State->Rect에서 채움)
	//    - ViewMode : 뷰포트별 독립 모드 (기본값 EViewMode::Lit)
	FSceneView SceneView;
	VC.BuildSceneView(SceneView);

	// 2. 렌더링 대상을 서브 영역으로 제한
	const FViewportRect& Rect = SceneView.ViewRect;
	if (Rect.Width <= 0 || Rect.Height <= 0) return;

	Renderer.GetFD3DDevice().SetSubViewport(Rect.X, Rect.Y, Rect.Width, Rect.Height);

	// 3. 이 뷰포트용 렌더 데이터 수집
	Bus.Clear();

	UWorld* World = Editor->GetWorld();
	const FEditorSettings& Settings = Editor->GetSettings();
	const FShowFlags& ShowFlags = Settings.ShowFlags;
	const EViewMode ViewMode = SceneView.ViewMode;

	Bus.SetViewProjection(Camera->GetViewMatrix(), Camera->GetProjectionMatrix());
	Bus.SetRenderSettings(ViewMode, ShowFlags);

	Collector.CollectWorld(World, ShowFlags, ViewMode, Bus);
	Collector.CollectGrid(Settings.GridSpacing, Settings.GridHalfLineCount, Bus);
	Collector.CollectGizmo(Editor->GetGizmo(), ShowFlags, Bus);
	Collector.CollectSelection(
		Editor->GetSelectionManager().GetSelectedActors(),
		ShowFlags, ViewMode, Bus);

	// 4. CPU 배처 데이터 준비 → GPU 드로우 (SetSubViewport 영역에만 출력됨)
	Renderer.PrepareBatchers(Bus);
	Renderer.Render(Bus);
}
