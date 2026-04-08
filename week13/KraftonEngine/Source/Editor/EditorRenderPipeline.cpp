#include "EditorRenderPipeline.h"
#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Render/Pipeline/Renderer.h"
#include "Viewport/Viewport.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/ProjectileMovementComponent.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Profiling/Stats.h"
#include "Profiling/GPUProfiler.h"

#include <cmath>

namespace
{
	void AddProjectileVelocityArrow(FRenderBus& Bus, const FVector& Start, const FVector& Velocity)
	{
		const float VelocityLength = Velocity.Length();
		if (VelocityLength <= FMath::Epsilon)
		{
			return;
		}

		const FVector Direction = Velocity / VelocityLength;
		const FVector End = Start + Velocity;
		const FColor ArrowColor(135, 206, 235);

		FDebugLineEntry Shaft;
		Shaft.Start = Start;
		Shaft.End = End;
		Shaft.Color = ArrowColor;
		Bus.AddDebugLineEntry(std::move(Shaft));

		const float HeadLength = Clamp(VelocityLength * 0.2f, 0.2f, 1.5f);
		FVector ReferenceUp = FVector(0.0f, 0.0f, 1.0f);
		if (std::abs(Direction.Dot(ReferenceUp)) > 0.98f)
		{
			ReferenceUp = FVector(0.0f, 1.0f, 0.0f);
		}

		const FVector Side = Direction.Cross(ReferenceUp).Normalized();
		const FVector Back = Direction * HeadLength;
		const FVector SideOffset = Side * (HeadLength * 0.45f);

		FDebugLineEntry HeadA;
		HeadA.Start = End;
		HeadA.End = End - Back + SideOffset;
		HeadA.Color = ArrowColor;
		Bus.AddDebugLineEntry(std::move(HeadA));

		FDebugLineEntry HeadB;
		HeadB.Start = End;
		HeadB.End = End - Back - SideOffset;
		HeadB.Color = ArrowColor;
		Bus.AddDebugLineEntry(std::move(HeadB));
	}
}

FEditorRenderPipeline::FEditorRenderPipeline(UEditorEngine* InEditor, FRenderer& InRenderer)
	: Editor(InEditor)
{
}

FEditorRenderPipeline::~FEditorRenderPipeline()
{
}

void FEditorRenderPipeline::OnSceneCleared()
{
	GPUOcclusion.InvalidateResults();
}

void FEditorRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
#if STATS
	FStatManager::Get().TakeSnapshot();
	FGPUProfiler::Get().TakeSnapshot();
	FGPUProfiler::Get().BeginFrame();
#endif

	for (FLevelEditorViewportClient* ViewportClient : Editor->GetLevelViewportClients())
	{
		SCOPE_STAT_CAT("RenderViewport", "2_Render");
		RenderViewport(ViewportClient, Renderer);
	}
	// 뷰포트별 오프스크린 렌더 (각 VP의 RT에 3D 씬 렌더)

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

	// GPU Occlusion 지연 초기화
	if (!GPUOcclusion.IsInitialized())
		GPUOcclusion.Initialize(Renderer.GetFD3DDevice().GetDevice());

	// 이전 프레임 Occlusion 결과 읽기 (staging → OccludedSet)
	GPUOcclusion.ReadbackResults(Ctx);

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
	Bus.SetOcclusionCulling(&GPUOcclusion);
	Bus.SetLODContext(World->PrepareLODContext());

	// 2. 프록시 + Batcher Entry를 ERenderPass별로 수집
	{
		SCOPE_STAT_CAT("Collector", "3_Collect");
		Collector.CollectWorld(World, Bus);

		if (UGizmoComponent* Gizmo = Editor->GetGizmo())
			Gizmo->UpdateAxisMask(Opts.ViewportType);

		Collector.CollectGrid(Opts.GridSpacing, Opts.GridHalfLineCount, Bus);
		Collector.CollectDebugDraw(World->GetDebugDrawQueue(), Bus);

		for (AActor* SelectedActor : Editor->GetSelectionManager().GetSelectedActors())
		{
			if (!SelectedActor || SelectedActor->GetWorld() != World)
			{
				continue;
			}

			for (UActorComponent* ActorComponent : SelectedActor->GetComponents())
			{
				UProjectileMovementComponent* ProjectileComponent = Cast<UProjectileMovementComponent>(ActorComponent);
				if (!ProjectileComponent)
				{
					continue;
				}

				const FVector PreviewVelocity = ProjectileComponent->GetPreviewVelocity();
				if (PreviewVelocity.Length() <= FMath::Epsilon)
				{
					continue;
				}

				USceneComponent* SourceComponent = ProjectileComponent->GetUpdatedComponent();
				if (!SourceComponent)
				{
					SourceComponent = SelectedActor->GetRootComponent();
				}
				if (!SourceComponent)
				{
					continue;
				}

				AddProjectileVelocityArrow(Bus, SourceComponent->GetWorldLocation(), PreviewVelocity);
			}
		}

		if (ShowFlags.bOctree)
			Collector.CollectOctreeDebug(World->GetOctree(), Bus);

		if (VC == Editor->GetActiveViewport())
			Collector.CollectOverlayText(Editor->GetOverlayStatSystem(), *Editor, Bus);
	}

	// 3. Batcher 준비
	{
		SCOPE_STAT_CAT("PrepareBatcher", "3_Collect");
		Renderer.PrepareBatchers(Bus);
	}

	// 4. GPU 드로우 콜 실행
	{
		SCOPE_STAT_CAT("Renderer.Render", "4_ExecutePass");
		Renderer.Render(Bus);
	}

	// 5. GPU Occlusion — DSV 언바인딩 후 Hi-Z 생성 + Occlusion Test 디스패치
	if (GPUOcclusion.IsInitialized())
	{
		SCOPE_STAT_CAT("GPUOcclusion", "4_ExecutePass");

		// DSV 언바인딩 (DepthSRV 읽기와 동시 바인딩 불가)
		ID3D11RenderTargetView* rtv = VP->GetRTV();
		Ctx->OMSetRenderTargets(1, &rtv, nullptr);

		GPUOcclusion.DispatchOcclusionTest(
			Ctx,
			VP->GetDepthSRV(),
			World->GetVisibleProxies(),
			Bus.GetView(), Bus.GetProj(),
			VP->GetWidth(), VP->GetHeight());
	}
}
