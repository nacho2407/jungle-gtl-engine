#include "EditorRenderPipeline.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Render/Renderer/Renderer.h"
#include "Viewport/Viewport.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "GameFramework/World.h"
#include "Core/Stats.h"
#include "Core/GPUProfiler.h"

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

	// 뷰포트별 오프스크린 렌더 (각 VP의 RT에 3D 씬 렌더)
	for (FLevelEditorViewportClient* VC : Editor->GetLevelViewportClients())
	{
		RenderViewport(VC->GetViewport(), VC->GetCamera(), Renderer);
	}

	// 스왑체인 백버퍼 복귀 → ImGui 합성 (Image(SRV)로 뷰포트 표시) → Present
	Renderer.BeginFrame();
	Editor->RenderUI(DeltaTime);
	Renderer.EndFrame();
}

void FEditorRenderPipeline::RenderViewport(FViewport* VP, UCameraComponent* Camera, FRenderer& Renderer)
{
	if (!Camera) return;

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

	// Bus 수집
	Bus.Clear();

	UWorld* World = Editor->GetWorld();
	const auto& Settings = Editor->GetSettings();
	const FShowFlags& ShowFlags = Settings.ShowFlags;
	EViewMode ViewMode = Settings.ViewMode;

	Bus.SetViewProjection(Camera->GetViewMatrix(), Camera->GetProjectionMatrix(),
		Camera->GetRightVector(), Camera->GetUpVector());
	Bus.SetRenderSettings(ViewMode, ShowFlags);

	Collector.CollectWorld(World, ShowFlags, ViewMode, Bus);
	Collector.CollectGrid(Settings.GridSpacing, Settings.GridHalfLineCount, Bus);
	Collector.CollectGizmo(Editor->GetGizmo(), ShowFlags, Bus);
	Collector.CollectSelection(
		Editor->GetSelectionManager().GetSelectedActors(),
		ShowFlags, ViewMode, Bus);

	// GPU 렌더
	Renderer.PrepareBatchers(Bus);
	Renderer.Render(Bus);
}
