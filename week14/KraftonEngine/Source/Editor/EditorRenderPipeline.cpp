#include "EditorRenderPipeline.h"
#include "Editor/EditorEngine.h"
#include "Editor/Viewport/Level/LevelEditorViewportClient.h"
#include "Editor/Viewport/EditorPreviewViewportClient.h"
#include "Render/Pipeline/Renderer.h"
#include "Render/Scene/FScene.h"
#include "Viewport/Viewport.h"
#include "Viewport/GameViewportClient.h"
#include "Component/Camera/CameraComponent.h"
#include "Component/Camera/CineCameraComponent.h"
#include "GameFramework/World.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "Profiling/Stats/Stats.h"
#include "Profiling/GPUProfiler.h"
#include "Engine/Render/Types/ForwardLightData.h"
#include "Engine/Render/Types/MinimalViewInfo.h"
#include "Component/Light/LightComponentBase.h"
#include "Physics/PhysicsScene.h"
#include "Core/ProjectSettings.h"
#include "Lua/LuaScriptManager.h"
#include "Math/MathUtils.h"

namespace
{
	void ApplyViewportResizeIfNeeded(FLevelEditorViewportClient* VC, FViewport* VP)
	{
		if (!VC || !VP)
		{
			return;
		}

		if (VP->ApplyPendingResize())
		{
			// 에디터 뷰포트 카메라의 aspect ratio는 viewport client가 관리합니다.
			VC->NotifyViewportResized(static_cast<int32>(VP->GetWidth()), static_cast<int32>(VP->GetHeight()));
		}
	}

	void SyncGameCameraAspectToViewport(APlayerCameraManager* CamManager, FViewport* VP, FMinimalViewInfo& InOutPOV)
	{
		if (!CamManager || !VP || VP->GetWidth() == 0 || VP->GetHeight() == 0)
		{
			return;
		}

		const int32 ViewportWidth = static_cast<int32>(VP->GetWidth());
		const int32 ViewportHeight = static_cast<int32>(VP->GetHeight());
		const float ViewportAspect = static_cast<float>(ViewportWidth) / static_cast<float>(ViewportHeight);

		// PIE 렌더는 editor pipeline을 타므로 standalone GameRenderPipeline처럼 게임 카메라 resize를 직접 보장합니다.
		if (UCameraComponent* ActiveCamera = CamManager->GetActiveCamera())
		{
			ActiveCamera->OnResize(ViewportWidth, ViewportHeight);
		}

		// CameraCachePOV는 WorldTick에서 이미 만들어졌을 수 있으므로 이번 프레임 POV도 즉시 보정합니다.
		InOutPOV.AspectRatio = ViewportAspect;
	}

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

	/**
	* @brief 뷰포트 렌더 타깃 초기화 색상 배열을 생성합니다.
	*/
	void BuildViewportClearColor(const FLinearColor& BackgroundColor, float OutClearColor[4])
	{
		// 렌더 타깃 초기화 색상은 0~1 범위 RGB와 불투명 alpha로 고정
		OutClearColor[0] = FMath::Clamp(BackgroundColor.R, 0.0f, 1.0f);
		OutClearColor[1] = FMath::Clamp(BackgroundColor.G, 0.0f, 1.0f);
		OutClearColor[2] = FMath::Clamp(BackgroundColor.B, 0.0f, 1.0f);
		OutClearColor[3] = 1.0f;
	}

	/**
	* @brief PIE 실행 뷰포트에서 숨길 에디터/디버그 전용 show flag를 비활성화합니다.
	*/
	void ApplyPIEViewportShowFlagOverride(FShowFlags& ShowFlags)
	{
		// 에디터 보조 표시
		ShowFlags.bGrid = false;
		ShowFlags.bWorldAxis = false;
		ShowFlags.bGizmo = false;
		ShowFlags.bBillboardText = false;
		ShowFlags.bBoundingVolume = false;
		ShowFlags.bOctree = false;
		ShowFlags.bShowShadowFrustum = false;

		// PIE 게임 화면을 가리는 디버그 표시
		ShowFlags.bDebugDraw = false;
		ShowFlags.bViewLightCulling = false;
		ShowFlags.bVisualize25DCulling = false;
		ShowFlags.bCollision = false;
		ShowFlags.bShowCollisionShape = false;

		// Physics Asset Editor 전용 표시
		ShowFlags.bPhysicsAssetBodies = false;
		ShowFlags.bPhysicsAssetConstraints = false;
		ShowFlags.bPhysicsAssetSkeleton = false;
		ShowFlags.bPhysicsAssetSimulation = false;
	}

	/**
	* @brief 지정 뷰포트가 현재 PIE 게임 뷰포트인지 확인합니다.
	*/
	bool IsPIEGameViewport(const UEditorEngine* Editor, const FViewport* Viewport)
	{
		return Editor && Editor->IsPIEGameViewport(Viewport);
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
	FScene& Scene = Editor->GetWorld()->GetScene();
	++Renderer.GetResources().GetShadowResourcesForScene(&Scene).Resources.FrameGeneration;

	for (FLevelEditorViewportClient* ViewportClient : Editor->GetLevelViewportClients())
	{
		if (!Editor->ShouldRenderViewportClient(ViewportClient))
		{
			continue;
		}

		SCOPE_STAT_CAT("RenderViewport", "2_Render");
		RenderViewport(ViewportClient, Renderer);
	}

	TArray<IEditorPreviewViewportClient*> PreviewViewportClients;
	Editor->CollectAssetEditorPreviewViewportClients(PreviewViewportClients);

	for (IEditorPreviewViewportClient* PreviewVC : PreviewViewportClients)
	{
		if (PreviewVC->IsRenderable() && PreviewVC->GetPreviewWorld())
		{
			FScene& PreviewScene = PreviewVC->GetPreviewWorld()->GetScene();
			++Renderer.GetResources().GetShadowResourcesForScene(&PreviewScene).Resources.FrameGeneration;

			SCOPE_STAT_CAT("RenderPreviewViewport", "2_Render");
			RenderPreviewViewport(PreviewVC, Renderer);
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

	// POV 산출 전에 지연 리사이즈를 적용해야 PIE 게임 카메라 aspect가 최신 RT 크기를 봅니다.
	ApplyViewportResizeIfNeeded(VC, VP);

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
				SyncGameCameraAspectToViewport(CamManager, VP, POV);
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
	VC->SetParticleStats(Output.ParticleStats);

	FScene& Scene = World->GetScene();

	// GPU 정렬 + 제출
	{
		SCOPE_STAT_CAT("Renderer.Render", "4_ExecutePass");
		Renderer.Render(Frame, World, Scene);
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

	Scene.ClearFrameData();
}

// ============================================================
// PrepareViewport — 지연 리사이즈 적용 + RT 클리어
// ============================================================
void FEditorRenderPipeline::PrepareViewport(FLevelEditorViewportClient* VC, FViewport* VP, ID3D11DeviceContext* Ctx)
{
	ApplyViewportResizeIfNeeded(VC, VP);

	float ClearColor[4];
	BuildViewportClearColor(Editor->GetSettings().ViewportBackgroundColor, ClearColor);
	VP->BeginRender(Ctx, ClearColor);
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
	FViewportRenderOptions RenderOptions = VC->GetRenderOptions();
	if (VC->IsPilotingActor())
	{
		RenderOptions.ShowFlags.bGizmo = false;
		Frame.HiddenEditorOnlyActorForView = VC->GetPilotedActor();
	}
	if (IsPIEGameViewport(Editor, VP))
	{
		// 실제 뷰포트 설정값은 그대로 두고 이번 렌더 프레임 복사본에만 PIE 전용 override 적용
		ApplyPIEViewportShowFlagOverride(RenderOptions.ShowFlags);
		RenderOptions.Saturation = FLuaScriptManager::GetRuntimeSaturation();
	}
	Frame.SetRenderOptions(RenderOptions);
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

		if (Flags.bShowCollisionShape)
		{
			if (FPhysicsScene* PhysicsScene = World->GetPhysicsScene())
			{
				PhysicsScene->CollectDebugRender(Scene);
			}
		}

		Collector.CollectDebugDraw(Frame, Scene);
	}

	// ── 3. 커맨드 일괄 생성 (프록시 + 동적) ──
	{
		SCOPE_STAT_CAT("BuildCommands", "3_Collect");
		Builder.BuildCommands(Frame, &Scene, Output);
	}
}

void FEditorRenderPipeline::RenderPreviewViewport(IEditorPreviewViewportClient* VC, FRenderer& Renderer)
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

	const FViewportRenderOptions RenderOptions = VC->GetRenderOptions();
	float ClearColor[4];
	BuildViewportClearColor(Editor->GetSettings().ViewportBackgroundColor, ClearColor);
	VP->BeginRender(Ctx, ClearColor);

	FMinimalViewInfo POV;
	VC->GetCameraView(POV);

	Frame.ClearViewportResources();
	Frame.SetViewportInfo(VP);
	Frame.SetCameraInfo(POV);
	Frame.WorldType = World->GetWorldType();
	Frame.HiddenEditorOnlyActorForView = nullptr;

	Frame.SetRenderOptions(RenderOptions);

	FScene& Scene = World->GetScene();

	FCollectOutput Output;
	FDrawCommandBuilder& Builder = Renderer.GetBuilder();
	Builder.BeginCollect(Frame);

	Collector.Collect(World, Frame, Output);
	VC->SubmitFrameDebugDraw();
	Collector.CollectDebugDraw(Frame, Scene);
	Builder.BuildCommands(Frame, &Scene, Output);

	Renderer.Render(Frame, World, Scene);
	Scene.ClearFrameData();
}
