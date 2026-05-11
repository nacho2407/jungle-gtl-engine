#include "EditorRenderPipeline.h"
#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Editor/Viewport/MeshEditorViewportClient.h"
#include "Render/Pipeline/Renderer.h"
#include "Render/Scene/FScene.h"
#include "Viewport/Viewport.h"
#include "Viewport/GameViewportClient.h"
#include "Component/CameraComponent.h"
#include "Component/CineCameraComponent.h"
#include "GameFramework/World.h"
#include "GameFramework/PlayerController.h"
#include "Profiling/Stats.h"
#include "Profiling/GPUProfiler.h"
#include "Engine/Render/Types/ForwardLightData.h"
#include "Engine/Render/Types/MinimalViewInfo.h"
#include "Component/Light/LightComponentBase.h"
#include "Core/ProjectSettings.h"
#include "Math/MathUtils.h"

namespace
{
	void ApplyLetterboxAspect(FMinimalViewInfo& POV, const FCameraLetterboxState& Letterbox, float ViewportWidth, float ViewportHeight)
	{
		if (!Letterbox.bEnabled || Letterbox.Amount <= 0.0f || ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
		{
			return;
		}

		const float Thickness = FMath::Clamp(Letterbox.Thickness * Letterbox.Amount, 0.0f, 0.49f);
		const float VisibleHeightScale = 1.0f - Thickness * 2.0f;
		if (VisibleHeightScale <= FMath::Epsilon)
		{
			return;
		}

		POV.AspectRatio = (ViewportWidth / ViewportHeight) / VisibleHeightScale;
	}
}

FEditorRenderPipeline::FEditorRenderPipeline(UEditorEngine* InEditor, FRenderer& InRenderer)
	: Editor(InEditor)
	, CachedDevice(InRenderer.GetFD3DDevice().GetDevice())
{
}

FEditorRenderPipeline::~FEditorRenderPipeline()
{
}

void FEditorRenderPipeline::OnSceneCleared()
{
	for (auto& [VC, Occlusion] : GPUOcclusionMap)
	{
		Occlusion->InvalidateResults();
	}

	for (FLevelEditorViewportClient* VC : Editor->GetLevelViewportClients())
	{
		VC->ClearLightViewOverride();
	}
}

FGPUOcclusionCulling& FEditorRenderPipeline::GetOcclusionForViewport(FLevelEditorViewportClient* VC)
{
	auto it = GPUOcclusionMap.find(VC);
	if (it != GPUOcclusionMap.end())
		return *it->second;

	auto ptr = std::make_unique<FGPUOcclusionCulling>();
	ptr->Initialize(CachedDevice);
	auto& ref = *ptr;
	GPUOcclusionMap.emplace(VC, std::move(ptr));
	return ref;
}

void FEditorRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
#if STATS
	FStatManager::Get().TakeSnapshot();
	FGPUProfiler::Get().TakeSnapshot();
	FGPUProfiler::Get().BeginFrame();
#endif

	// 이전 프레임 시각화 데이터 readback + 디버그 라인 제출
	Renderer.SubmitCullingDebugLines(Editor->GetWorld());

	// Shadow depth는 라이트 시점 → 뷰포트 무관. 프레임당 1회만 렌더링.
	++Renderer.GetResources().ShadowResources.FrameGeneration;

	for (FLevelEditorViewportClient* ViewportClient : Editor->GetLevelViewportClients())
	{
		if (!Editor->ShouldRenderViewportClient(ViewportClient))
		{
			continue;
		}

		SCOPE_STAT_CAT("RenderViewport", "2_Render");
		RenderViewport(ViewportClient, Renderer);
	}

	if (FMeshEditorViewportClient* MeshVC = Editor->GetMeshEditorViewportClient())
	{
		if (MeshVC->IsRenderable())
		{
			SCOPE_STAT_CAT("RenderMeshViewport", "2_Render");
			RenderMeshViewport(MeshVC, Renderer);
		}
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
	FViewport* VP = VC->GetViewport();
	if (!VP) return;

	ID3D11DeviceContext* Ctx = Renderer.GetFD3DDevice().GetDeviceContext();
	if (!Ctx) return;

	UWorld* World = Editor->GetWorld();
	if (!World) return;

	// ── 카메라 POV 통화 결정 ──────────────────────────────────────
	// 기본은 viewport 자체 카메라. PIE possessed 모드의 게임 뷰포트만 게임 ActiveCamera 사용.
	FMinimalViewInfo POV;
	VC->GetCameraView(POV);

	bool bShouldUseGameCamera = false;
	if (Editor && Editor->IsPIEPossessedMode())
	{
		if (UGameViewportClient* GVC = Editor->GetGameViewportClient())
		{
			bShouldUseGameCamera = GVC->GetViewport() == VP;
		}
	}

	if (bShouldUseGameCamera)
	{
		// E.2/2: PC 경로 — World 의 GetCameraManager 의존 제거.
		// shake/blend 가 적용된 최종 POV 가 필요하므로 GetCameraCachePOV 사용
		// (UpdateCamera 가 매 프레임 채워 둠).
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (APlayerCameraManager* CamManager = PC->GetPlayerCameraManager())
			{
				CamManager->GetCameraCachePOV(POV);
			}
		}
	}

	FGPUOcclusionCulling& GPUOcclusion = GetOcclusionForViewport(VC);

	// GPU Occlusion — 이전 프레임 결과 읽기 (이 뷰포트 전용)
	GPUOcclusion.ReadbackResults(Ctx);

	PrepareViewport(VC, VP, Ctx);
	BuildFrame(VC, POV, VP, World);

	FCollectOutput Output;
	CollectCommands(VC, World, Renderer, Output);

	FScene& Scene = World->GetScene();

	// GPU 정렬 + 제출
	{
		SCOPE_STAT_CAT("Renderer.Render", "4_ExecutePass");
		Renderer.Render(Frame, Scene);
	}

	// GPU Occlusion — Render 후 DepthBuffer가 유효할 때 디스패치 (이 뷰포트 전용)
	{
		SCOPE_STAT_CAT("GPUOcclusion", "4_ExecutePass");
		GPUOcclusion.DispatchOcclusionTest(
			Ctx,
			VP->GetDepthCopySRV(),
			Output.FrustumVisibleProxies,
			Frame.View, Frame.Proj,
			VP->GetWidth(), VP->GetHeight());
	}
}

// ============================================================
// PrepareViewport — 지연 리사이즈 적용 + RT 클리어
// ============================================================
void FEditorRenderPipeline::PrepareViewport(FLevelEditorViewportClient* VC, FViewport* VP, ID3D11DeviceContext* Ctx)
{
	if (VP->ApplyPendingResize())
	{
		// 컴포넌트 OnResize 는 viewport client 가 책임진다. 파이프라인은 컴포넌트를 모름.
		VC->NotifyViewportResized(static_cast<int32>(VP->GetWidth()), static_cast<int32>(VP->GetHeight()));
	}
	VP->BeginRender(Ctx);
}

// ============================================================
// BuildFrame — FFrameContext 일괄 설정 (POV 통화 입력)
// ============================================================
void FEditorRenderPipeline::BuildFrame(FLevelEditorViewportClient* VC, const FMinimalViewInfo& POV, FViewport* VP, UWorld* World)
{
	Frame.ClearViewportResources();
	Frame.SetViewportInfo(VP);

	// PC 가 PlayerCameraManager owner — 그쪽으로부터 camera post state read.
	APlayerController* PC = World->GetFirstPlayerController();
	APlayerCameraManager* CamManager = PC ? PC->GetPlayerCameraManager() : nullptr;
	Frame.CameraFade.bEnabled = CamManager ? CamManager->IsFadeEnabled() : false;
	if (Frame.CameraFade.bEnabled)
	{
		Frame.CameraFade.Color = CamManager->GetFadeColor();
		Frame.CameraFade.Amount = CamManager->GetFadeAmount();
	}

	Frame.CameraVignette.bEnabled = CamManager ? CamManager->IsVignetteEnabled() : false;
	if (Frame.CameraVignette.bEnabled)
	{
		Frame.CameraVignette.Intensity = CamManager->GetVignetteIntensity();
		Frame.CameraVignette.Radius = CamManager->GetVignetteRadius();
		Frame.CameraVignette.Softness = CamManager->GetVignetteSoftness();
		Frame.CameraVignette.Color = CamManager->GetVignetteColor();
	}

	UCameraComponent* ActiveCamera = CamManager ? CamManager->GetActiveCamera() : nullptr;
	if (UCineCameraComponent* CineCamera = Cast<UCineCameraComponent>(ActiveCamera))
	{
		const FCineLetterboxSettings& LetterboxSettings = CineCamera->GetLetterboxSettings();
		Frame.CameraLetterbox.bEnabled = LetterboxSettings.bEnabled;
		if (Frame.CameraLetterbox.bEnabled)
		{
			Frame.CameraLetterbox.Amount = LetterboxSettings.Amount;
			Frame.CameraLetterbox.Thickness = LetterboxSettings.Thickness;
			Frame.CameraLetterbox.Color = LetterboxSettings.Color;
		}
	}
	else
	{
		Frame.CameraLetterbox.bEnabled = false;
	}

	FMinimalViewInfo RenderPOV = POV;
	ApplyLetterboxAspect(RenderPOV, Frame.CameraLetterbox, Frame.ViewportWidth, Frame.ViewportHeight);
	Frame.SetCameraInfo(RenderPOV);

	// Light View Override — 라이트 시점으로 View/Proj 교체.
	// Directional CSM 은 viewer POV 의 frustum 으로 cascade 분할 → 위에서 추출한 POV 를 그대로 위임.
	if (VC->IsViewingFromLight())
	{
		ULightComponentBase* Light = VC->GetLightViewOverride();
		if (!Light || !Light->GetOwner())
		{
			VC->ClearLightViewOverride();
		}
		else
		{
			FLightViewProjResult LVP;
			if (Light->GetLightViewProj(LVP, &RenderPOV, VC->GetPointLightFaceIndex()))
			{
				Frame.View = LVP.View;
				Frame.Proj = LVP.Proj;
				Frame.bIsOrtho = LVP.bIsOrtho;
				Frame.CameraPosition = Light->GetWorldLocation();
				Frame.CameraForward = Light->GetForwardVector();
				Frame.FrustumVolume.UpdateFromMatrix(Frame.View * Frame.Proj);
			}
		}
	}

	Frame.bIsLightView = VC->IsViewingFromLight();
	Frame.WorldType = World->GetWorldType();
	Frame.SetRenderOptions(VC->GetRenderOptions());
	Frame.OcclusionCulling = &GetOcclusionForViewport(VC);
	Frame.LODContext = World->PrepareLODContext();

	// Cursor position relative to viewport (for 2.5D culling visualization)
	if (!VC->GetCursorViewportPosition(Frame.CursorViewportX, Frame.CursorViewportY))
	{
		Frame.CursorViewportX = UINT32_MAX;
		Frame.CursorViewportY = UINT32_MAX;
	}

}

// ============================================================
// CollectCommands — Scene 데이터 주입 + DrawCommand 생성
// ============================================================
//
// 3단계로 구성:
//   1. Proxy   — frustum cull → DrawCommand 즉시 생성 (메시/폰트/데칼)
//   2. Debug   — Scene에 디버그 데이터 주입 (Grid, DebugDraw, Octree, ShadowFrustum)
//   3. UI      — Scene에 오버레이 텍스트 주입
//
// 마지막에 BuildDynamicCommands가 Scene 주입 데이터를 DrawCommand로 변환.

void FEditorRenderPipeline::CollectCommands(FLevelEditorViewportClient* VC, UWorld* World, FRenderer& Renderer, FCollectOutput& Output)
{
	SCOPE_STAT_CAT("Collector", "3_Collect");

	FScene& Scene = World->GetScene();
	Scene.ClearFrameData();

	FDrawCommandBuilder& Builder = Renderer.GetBuilder();
	Builder.BeginCollect(Frame);

	const FShowFlags& Flags = Frame.RenderOptions.ShowFlags;

	// ── 1. 데이터 수집: frustum cull + visibility/occlusion 필터 ──
	{
		SCOPE_STAT_CAT("Collect", "3_Collect");
		Collector.Collect(World, Frame, Output);
	}

	// ── 2. Debug: Scene에 디버그 데이터 주입 ──
	{
		SCOPE_STAT_CAT("CollectDebug", "3_Collect");
		Collector.CollectGrid(Frame.RenderOptions.GridSpacing, Frame.RenderOptions.GridHalfLineCount, Scene);

		if (Flags.bShowShadowFrustum)
			Scene.SubmitShadowFrustumDebug(World, Frame);

		if (Flags.bOctree)
			Collector.CollectOctreeDebug(World->GetOctree(), Scene);

		Collector.CollectDebugDraw(Frame, Scene);
	}

	// ── 3. 커맨드 일괄 생성 (프록시 + 동적) ──
	{
		SCOPE_STAT_CAT("BuildCommands", "3_Collect");
		Builder.BuildCommands(Frame, &Scene, Output);
	}
}

void FEditorRenderPipeline::RenderMeshViewport(FMeshEditorViewportClient* VC, FRenderer& Renderer)
{
	FViewport* VP = VC->GetViewport();
	UWorld* World = VC->GetPreviewWorld();
	if (!VP || !World) return;

	ID3D11DeviceContext* Ctx = Renderer.GetFD3DDevice().GetDeviceContext();
	if (!Ctx) return;

	if (VP->ApplyPendingResize())
	{
		VC->NotifyViewportResized(static_cast<int32>(VP->GetWidth()), static_cast<int32>(VP->GetHeight()));
	}

	const float ClearColor[4] = { 0.12f, 0.12f, 0.13f, 1.0f };
	VP->BeginRender(Ctx, ClearColor);

	FMinimalViewInfo POV;
	VC->GetCameraView(POV);

	Frame.ClearViewportResources();
	Frame.SetViewportInfo(VP);
	Frame.SetCameraInfo(POV);
	Frame.WorldType = World->GetWorldType();

	Frame.SetRenderOptions(VC->GetRenderOptions());

	FScene& Scene = World->GetScene();
	Scene.ClearFrameData();

	FCollectOutput Output;
	FDrawCommandBuilder& Builder = Renderer.GetBuilder();
	Builder.BeginCollect(Frame);

	Collector.Collect(World, Frame, Output);
	Builder.BuildCommands(Frame, &Scene, Output);

	Renderer.Render(Frame, Scene);
}
