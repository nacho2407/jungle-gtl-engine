#include "EditorRenderPipeline.h"
#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Render/Pipeline/Renderer.h"
#include "Render/Scene/FScene.h"
#include "Viewport/Viewport.h"
#include "Component/CameraComponent.h"
#include "GameFramework/World.h"
#include "Profiling/Stats.h"
#include "Profiling/GPUProfiler.h"
#include "Engine/Render/Types/ForwardLightData.h"
#include "Component/Light/LightComponentBase.h"
#include "Editor/Settings/ProjectSettings.h"

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

	// Non-PSM: 전체 1회 shadow bake (뷰포트 루프 전)
	const auto& Shadow = FProjectSettings::Get().Shadow;
	if (Shadow.bEnabled && !Shadow.bPSM)
	{
		SCOPE_STAT_CAT("GlobalShadows", "4_ExecutePass");
		// Renderer.RenderGlobalShadows(Editor->GetWorld()->GetScene(), &Editor->GetWorld()->GetPartition());
	}

	for (FLevelEditorViewportClient* ViewportClient : Editor->GetLevelViewportClients())
	{
		if (!Editor->ShouldRenderViewportClient(ViewportClient))
		{
			continue;
		}

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

	FGPUOcclusionCulling& GPUOcclusion = GetOcclusionForViewport(VC);

	// GPU Occlusion — 이전 프레임 결과 읽기 (이 뷰포트 전용)
	GPUOcclusion.ReadbackResults(Ctx);

	PrepareViewport(VP, Camera, Ctx);
	BuildFrame(VC, Camera, VP, World);

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
void FEditorRenderPipeline::PrepareViewport(FViewport* VP, UCameraComponent* Camera, ID3D11DeviceContext* Ctx)
{
	if (VP->ApplyPendingResize())
	{
		Camera->OnResize(static_cast<int32>(VP->GetWidth()), static_cast<int32>(VP->GetHeight()));
	}
	VP->BeginRender(Ctx);
}

// ============================================================
// BuildFrame — FFrameContext 일괄 설정
// ============================================================
void FEditorRenderPipeline::BuildFrame(FLevelEditorViewportClient* VC, UCameraComponent* Camera, FViewport* VP, UWorld* World)
{
	Frame.ClearViewportResources();
	Frame.SetCameraInfo(Camera);

	// Light View Override — 라이트 시점으로 View/Proj 교체
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
			if (Light->GetLightViewProj(LVP, Camera, VC->GetPointLightFaceIndex()))
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
	Frame.SetRenderOptions(VC->GetRenderOptions());
	Frame.SetViewportInfo(VP);
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
	Builder.BeginCollect(Frame, Scene.GetProxyCount());

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

	// ── 3. UI: 오버레이 텍스트 ──
	if (VC == Editor->GetActiveViewport())
		Collector.CollectOverlayText(Editor->GetOverlayStatSystem(), *Editor, Scene);

	// ── 4. 커맨드 일괄 생성 (프록시 + 동적) ──
	{
		SCOPE_STAT_CAT("BuildCommands", "3_Collect");
		Builder.BuildCommands(Frame, &Scene, Output);
	}
}

