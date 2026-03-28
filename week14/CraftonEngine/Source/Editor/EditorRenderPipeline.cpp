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
		Camera->GetRightVector(), Camera->GetUpVector());
	Bus.SetRenderSettings(ViewMode, ShowFlags);
	if (VP)
	{
		Bus.SetViewportSize(static_cast<float>(VP->GetWidth()), static_cast<float>(VP->GetHeight()));
	}
	Collector.CollectWorld(World, ShowFlags, ViewMode, Bus);
	Collector.CollectGrid(Opts.GridSpacing, Opts.GridHalfLineCount, Bus);
	Collector.CollectGizmo(Editor->GetGizmo(), ShowFlags, Bus);
	Collector.CollectSelection(
		Editor->GetSelectionManager().GetSelectedActors(),
		ShowFlags, ViewMode, Bus);

	const TArray<FString> OverlayLines = Editor->GetOverlayStatSystem().BuildLines(*Editor);
	if (!OverlayLines.empty() && VP && VC == Editor->GetActiveViewport())
	{
		const float StartX = 16.0f;
		const float StartY = 25.0f;
		const float LineHeight = 20.0f;

		for (size_t i = 0; i < OverlayLines.size(); ++i)
		{
			FRenderCommand Cmd = {};
			Cmd.Type = ERenderCommandType::Font;
			Cmd.BlendState = EBlendState::AlphaBlend;
			Cmd.DepthStencilState = EDepthStencilState::NoDepth;

			Cmd.Constants.Font.Text = &OverlayLines[i];
			Cmd.Constants.Font.Font = nullptr;
			Cmd.Constants.Font.Scale = 1.0f;
			Cmd.Constants.Font.bScreenSpace = 1;
			Cmd.Constants.Font.ScreenPosition = FVector2(
				StartX,
				StartY + static_cast<float>(i) * LineHeight
			);

			Bus.AddCommand(ERenderPass::OverlayFont, std::move(Cmd));
		}
	}

	// GPU 렌더
	Renderer.PrepareBatchers(Bus);
	Renderer.Render(Bus);
}
