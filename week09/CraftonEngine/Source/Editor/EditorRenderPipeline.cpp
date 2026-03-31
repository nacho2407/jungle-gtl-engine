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
#include "Editor/Subsystem/OverlayStatSystem.h"

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
#endif

	// 뷰포트별 오프스크린 렌더 (각 VP의 RT에 3D 씬 렌더)
	for (FLevelEditorViewportClient* VC : Editor->GetLevelViewportClients())
	{
		RenderViewport(VC, Renderer);
	}

	// 스왑체인 백버퍼 복귀 → ImGui 합성 (Image(SRV)로 뷰포트 표시) → Present
	Renderer.BeginFrame();
	Editor->RenderUI(DeltaTime);
	Renderer.EndFrame();
}

void FEditorRenderPipeline::RenderViewport(FLevelEditorViewportClient* VC, FRenderer& Renderer)
{
	UCameraComponent* Camera = VC->GetCamera();
	if (!Camera) return;

	FViewport* VP = VC->GetViewport();
	ID3D11DeviceContext* Ctx = Renderer.GetFD3DDevice().GetDeviceContext();

	// 지연 리사이즈 적용 (ImGui에서 요청된 크기 변경을 렌더 직전에 반영)
	if (VP)
	{
		if (VP->ApplyPendingResize())
		{
			Camera->OnResize(static_cast<int32>(VP->GetWidth()), static_cast<int32>(VP->GetHeight()));
		}
	}

	// 오프스크린 RT 바인딩 (VP가 유효하면 해당 RT로, 아니면 스왑체인 그대로 사용)
	if (VP && VP->GetRTV())
	{
		const float ClearColor[4] = { 0.25f, 0.25f, 0.25f, 1.0f };
		ID3D11RenderTargetView* RTV = VP->GetRTV();

		Ctx->ClearRenderTargetView(RTV, ClearColor);
		Ctx->ClearDepthStencilView(VP->GetDSV(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		Ctx->OMSetRenderTargets(1, &RTV, VP->GetDSV());

		D3D11_VIEWPORT VPRect = VP->GetViewportRect();
		Ctx->RSSetViewports(1, &VPRect);
	}

	// 뷰포트별 렌더 옵션 사용
	const FViewportRenderOptions& Opts = VC->GetRenderOptions();
	const FShowFlags& ShowFlags = Opts.ShowFlags;
	EViewMode ViewMode = Opts.ViewMode;

	// Bus 수집
	Bus.Clear();

	UWorld* World = Editor->GetWorld();

	Bus.SetViewProjection(Camera->GetViewMatrix(), Camera->GetProjectionMatrix(),
		Camera->GetForwardVector(), Camera->GetRightVector(), Camera->GetUpVector());
	Bus.SetCameraInfo(Camera->IsOrthogonal(), Camera->GetOrthoWidth());
	Bus.SetRenderSettings(ViewMode, ShowFlags);
	if (VP)
	{
		Bus.SetViewportSize(static_cast<float>(VP->GetWidth()), static_cast<float>(VP->GetHeight()));
		Bus.SetViewportResources(VP->GetRTV(), VP->GetDSV(), VP->GetStencilSRV());
	}

	// RenderCommand를 ERenderPass별로 수집. 각 컴포넌트가 자신의 렌더 엔트리를 Bus에 직접 추가.
	Collector.CollectWorld(World, Editor->GetSelectionManager().GetSelectedActors(), Bus);
	Collector.CollectGrid(Opts.GridSpacing, Opts.GridHalfLineCount, Bus);

	UGizmoComponent* Gizmo = Editor->GetGizmo();
	if (Gizmo) Gizmo->CollectRender(Bus);

	const TArray<FOverlayStatLine> OverlayLines = Editor->GetOverlayStatSystem().BuildLines(*Editor);
	if (!OverlayLines.empty() && VP && VC == Editor->GetActiveViewport())
	{
		TArray<FScreenTextItem> TextItems;
		TextItems.reserve(OverlayLines.size());
		for (const FOverlayStatLine& Line : OverlayLines)
		{
			TextItems.push_back({ &Line.Text, Line.ScreenPosition });
		}

		Collector.CollectOverlayText(TextItems, Editor->GetOverlayStatSystem().GetLayout().TextScale, Bus);
	}

	//============

	// GPU 렌더
	Renderer.PrepareBatchers(Bus);
	Renderer.Render(Bus);
}
