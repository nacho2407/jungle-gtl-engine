#include "EditorRenderPipeline.h"

#include "Editor/EditorEngine.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/ViewportCamera.h"
#include "Render/Renderer/Renderer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"
#include "Core/Logging/Stats.h"
#include "Core/Logging/GPUProfiler.h"
#include "Core/Logging/Log.h"
#include "Runtime/SceneView.h"
#include "Engine/Component/GizmoComponent.h"
#include "Engine/Component/SkeletalMeshComponent.h"
#include "GameFramework/PrimitiveActors.h"
#include "Asset/StaticMesh.h"
#include "Render/Resource/Buffer.h"
#include "Render/Resource/Material.h"
#include "Render/Scene/RenderCommand.h"
#include "Math/Utils.h"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace
{
    void ApplyPIECameraViewEffectsToBus(APlayerController* PlayerController, FRenderBus& Bus)
    {
        APlayerCameraManager* CameraManager = PlayerController
            ? PlayerController->GetPlayerCameraManager()
            : nullptr;
        if (!CameraManager)
        {
            return;
        }

        const FMinimalViewInfo& ViewInfo = CameraManager->GetCameraView();
        const FCameraPostProcessSettings& PostProcess = ViewInfo.PostProcessSettings;
        if (PostProcess.bVignetteEnabled)
        {
            Bus.SetVignette(
                PostProcess.VignetteIntensity,
                PostProcess.VignetteRadius,
                PostProcess.VignetteSmoothness,
                PostProcess.VignetteColor);
        }
        if (CameraManager->HasLetterbox())
        {
            Bus.SetLetterbox(CameraManager->GetLetterboxTargetAspect(), CameraManager->GetLetterboxAmount());
        }
        if (CameraManager->HasVisibleFade())
        {
            Bus.SetCameraFade(CameraManager->GetFadeColor().ToVector4(), CameraManager->GetFadeAlpha());
        }
    }
}

FEditorRenderPipeline::FEditorRenderPipeline(UEditorEngine* InEditor, FRenderer& InRenderer) : Editor(InEditor)
{
    Collector.Initialize(InRenderer.GetFD3DDevice().GetDevice());
    ViewportCullingStats.resize(FEditorViewportLayout::MaxViewports);
	ViewportDecalStats.resize(FEditorViewportLayout::MaxViewports);
	ViewportLightStats.resize(FEditorViewportLayout::MaxViewports);
}

FEditorRenderPipeline::~FEditorRenderPipeline() { Collector.Release(); }

void FEditorRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
    const auto PipelineStart = std::chrono::steady_clock::now();
#if STATS
    FStatManager::Get().TakeSnapshot();
    FGPUProfiler::Get().TakeSnapshot();

    {
        const TArray<FStatEntry>& GpuSnapshot = FGPUProfiler::Get().GetGPUSnapshot();
        double GpuTotalMs = 0.0;
        double GpuTopMs = 0.0;
        const char* GpuTopName = "";
        double GpuTopPassMs[3] = {};
        const char* GpuTopPassName[3] = { "", "", "" };
        for (const FStatEntry& Entry : GpuSnapshot)
        {
            const double EntryMs = Entry.TotalTime * 1000.0;
            GpuTotalMs += EntryMs;
            if (EntryMs > GpuTopMs)
            {
                GpuTopMs = EntryMs;
                GpuTopName = Entry.Name ? Entry.Name : "";
            }

            const char* EntryName = Entry.Name ? Entry.Name : "";
            const bool bIsFrameWrapper = std::strcmp(EntryName, "EditorFrameGPU") == 0;
            if (!bIsFrameWrapper && EntryMs > 0.0)
            {
                for (int32 TopIndex = 0; TopIndex < 3; ++TopIndex)
                {
                    if (EntryMs > GpuTopPassMs[TopIndex])
                    {
                        for (int32 ShiftIndex = 2; ShiftIndex > TopIndex; --ShiftIndex)
                        {
                            GpuTopPassMs[ShiftIndex] = GpuTopPassMs[ShiftIndex - 1];
                            GpuTopPassName[ShiftIndex] = GpuTopPassName[ShiftIndex - 1];
                        }

                        GpuTopPassMs[TopIndex] = EntryMs;
                        GpuTopPassName[TopIndex] = EntryName;
                        break;
                    }
                }
            }
        }

        static std::chrono::steady_clock::time_point LastGpuPerfLogTime = {};
        const auto GpuPerfNow = std::chrono::steady_clock::now();
        const bool bCanLogGpu =
            LastGpuPerfLogTime.time_since_epoch().count() == 0 ||
            std::chrono::duration<double>(GpuPerfNow - LastGpuPerfLogTime).count() >= 0.5;
        if (GpuSnapshot.size() > 0 && GpuTotalMs >= 2.0 && bCanLogGpu)
        {
            LastGpuPerfLogTime = GpuPerfNow;
            UE_LOG("[GPUFramePerf] Total=%.2fms Top=%s TopMs=%.2f TopPass=%s %.2fms | %s %.2fms | %s %.2fms Samples=%zu State=%d FocusedWorld=%p",
                GpuTotalMs,
                GpuTopName,
                GpuTopMs,
                GpuTopPassName[0],
                GpuTopPassMs[0],
                GpuTopPassName[1],
                GpuTopPassMs[1],
                GpuTopPassName[2],
                GpuTopPassMs[2],
                GpuSnapshot.size(),
                static_cast<int32>(Editor->GetEditorState()),
                static_cast<void*>(Editor->GetFocusedWorld()));
        }
    }
#endif
    const auto StatsEnd = std::chrono::steady_clock::now();

    for (FRenderCollector::FCullingStats& Stats : ViewportCullingStats)
    {
        Stats = {};
    }
    const auto ResetStatsEnd = std::chrono::steady_clock::now();

    if (!Editor->GetFocusedWorld())
        return;

    // 1회: 전체 백버퍼 클리어 (색상 + 깊이/스텐실)
    const auto BeginFrameStart = std::chrono::steady_clock::now();
    Renderer.BeginFrame();
    const auto BeginFrameEnd = std::chrono::steady_clock::now();

    std::chrono::steady_clock::time_point ViewportsStart;
    std::chrono::steady_clock::time_point ViewportsEnd;
    std::chrono::steady_clock::time_point BackBufferStart;
    std::chrono::steady_clock::time_point BackBufferEnd;
    std::chrono::steady_clock::time_point UIStart;
    std::chrono::steady_clock::time_point UIEnd;
    {
        GPU_SCOPE_STAT("EditorFrameGPU");

        // 4개 뷰포트를 순서대로 렌더링
        ViewportsStart = std::chrono::steady_clock::now();
        for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
        {
            RenderViewport(Renderer, i);
        }

		RenderViewerViewport(Renderer);
        ViewportsEnd = std::chrono::steady_clock::now();

        BackBufferStart = std::chrono::steady_clock::now();
        Renderer.UseBackBufferRenderTargets();
        BackBufferEnd = std::chrono::steady_clock::now();

        // ImGui UI 오버레이
        UIStart = std::chrono::steady_clock::now();
        Editor->RenderUI(DeltaTime);
        UIEnd = std::chrono::steady_clock::now();
    }

    const auto EndFrameStart = std::chrono::steady_clock::now();
    Renderer.EndFrame();
    const auto EndFrameEnd = std::chrono::steady_clock::now();

#if STATS
    static std::chrono::steady_clock::time_point LastPipelinePerfLogTime = {};
    const double TotalMs = std::chrono::duration<double, std::milli>(EndFrameEnd - PipelineStart).count();
    const auto Now = EndFrameEnd;
    const bool bCanLog =
        LastPipelinePerfLogTime.time_since_epoch().count() == 0 ||
        std::chrono::duration<double>(Now - LastPipelinePerfLogTime).count() >= 0.25;
    if (TotalMs >= 18.0 && bCanLog)
    {
        LastPipelinePerfLogTime = Now;
        auto ToMs = [](std::chrono::steady_clock::duration Duration)
        {
            return std::chrono::duration<double, std::milli>(Duration).count();
        };
        UE_LOG("[EditorPipelinePerf] Total=%.2fms Stats=%.2fms Reset=%.2fms BeginFrame=%.2fms Viewports=%.2fms BackBuffer=%.2fms RenderUI=%.2fms EndFrame=%.2fms State=%d FocusedWorld=%p",
               TotalMs,
               ToMs(StatsEnd - PipelineStart),
               ToMs(ResetStatsEnd - StatsEnd),
               ToMs(BeginFrameEnd - BeginFrameStart),
               ToMs(ViewportsEnd - ViewportsStart),
               ToMs(BackBufferEnd - BackBufferStart),
               ToMs(UIEnd - UIStart),
               ToMs(EndFrameEnd - EndFrameStart),
               static_cast<int32>(Editor->GetEditorState()),
               static_cast<void*>(Editor->GetFocusedWorld()));
    }
#endif
}

void FEditorRenderPipeline::RenderViewport(FRenderer& Renderer, int32 ViewportIndex)
{
    const auto ViewportRenderStart = std::chrono::steady_clock::now();
    if (Editor->GetEditorState() != EViewportPlayState::Editing)
    {
        const int32 ActivePIEViewportIndex = Editor->GetPIESession().GetActiveViewportIndex();
        if (ActivePIEViewportIndex >= 0 && ViewportIndex != ActivePIEViewportIndex)
        {
            return;
        }
    }

    FEditorViewportClient* VC = Editor->GetViewportLayout().GetViewportClient(ViewportIndex);

    // 1. 이 뷰포트의 SceneView 빌드
    //    - ViewRect : 화면 내 서브 영역 (BuildSceneView가 State->Rect에서 채움)
    //    - ViewMode : 뷰포트별 독립 모드 (기본값 EViewMode::Lit)
    FSceneView SceneView;
    VC->BuildSceneView(SceneView);

    // 2. 렌더링 대상을 서브 영역으로 제한
    const FViewportRect& Rect = SceneView.ViewRect;
    if (Rect.Width <= 0 || Rect.Height <= 0)
        return;

    // Avoid recreating the full viewport render-target stack every frame while
    // layout animation is resizing panes. The UI can scale the last SRV until
    // the transition lands, then we render once at the final size.
    if (Editor->GetViewportLayout().IsLayoutTransitionActive())
        return;

    FSceneViewport& SceneViewport = Editor->GetViewportLayout().GetSceneViewport(ViewportIndex);
    
	// Width, Height 변경 여부에 따라 Resource 버퍼 재생성
	// 만약 최소화 등의 상황으로 (H, W) == (0, 0) 일 경우 Render 안함
	FViewportRenderResource& ViewportResource = Editor->GetRenderer().AcquireViewportResource(Rect.Width, Rect.Height, ViewportIndex);
    SceneViewport.SetRenderTargetSet(&ViewportResource.GetView());

    // Viewport 별 버퍼 클리어 및 Renderer 버퍼 세팅
    Renderer.BeginViewportFrame(SceneViewport.GetViewportRenderTargets());

    // 3. 이 뷰포트용 렌더 데이터 수집
    Bus.Clear();

    // 각 뷰포트는 자신이 참조하는 월드를 렌더링합니다.
    UWorld*                World = VC->GetFocusedWorld();
    const FEditorSettings& Settings = Editor->GetSettings();
    const FShowFlags&      ShowFlags = Settings.ShowFlags;
    const EViewMode        ViewMode = SceneView.ViewMode;

    const FViewportCamera* Camera = VC->GetRenderCamera();
    if (Camera == nullptr)
        return;

    Bus.SetViewProjection(SceneView.ViewMatrix, SceneView.ProjectionMatrix, Camera->GetNearPlane(), Camera->GetFarPlane());
    Bus.SetRenderSettings(ViewMode, ShowFlags);
    Bus.SetLightCullMode(SceneView.LightCullMode);
    Bus.SetShadowFilterMode(Settings.ShadowFilterMode);
	Bus.SetViewportSize(FVector2(static_cast<float>(Rect.Width), static_cast<float>(Rect.Height)));
    Bus.SetViewportOrigin(FVector2(0.0f, 0.0f));
    Bus.SetFXAAEnabled(Settings.bEnableFXAA && !SceneView.bOrthographic);
    Bus.SetCascadeVis(VC->GetViewportState()->bShowCascadeVis);
    if (Editor->GetEditorState() != EViewportPlayState::Editing)
    {
        ApplyPIECameraViewEffectsToBus(VC->GetPIEPlayerController(), Bus);
    }

	if (World->IsSandervistanActivated())
	{
        Bus.bSandevistanEnabled = true;
        Bus.SandevistanIntensity = 1.0f;
	}
	else
	{
        Bus.bSandevistanEnabled = false;
        Bus.SandevistanIntensity = 0.0f;
	}
	
	const FFrustum& ViewFrustum = SceneView.CameraFrustum;
	const bool bDrawEditorViewportHelpers = VC->AllowsEditorWorldControl();
	Collector.CollectWorld(World, ShowFlags, ViewMode, Bus, &ViewFrustum, bDrawEditorViewportHelpers);
    ViewportCullingStats[ViewportIndex] = Collector.GetLastCullingStats();
    ViewportDecalStats[ViewportIndex] = Collector.GetLastDecalStats();
    ViewportLightStats[ViewportIndex] = Collector.GetLastLightStats();

	// 순수 편집 뷰포트와 PIE Eject는 모두 Editor viewport setting을 따릅니다.
	if (bDrawEditorViewportHelpers)
    {
        Collector.CollectGrid(Settings.GridSpacing, Settings.GridHalfLineCount, Bus, SceneView.bOrthographic);
        const FWorldContext* Ctx = Editor->GetWorldContextFromWorld(World);
        if (UGizmoComponent* Gizmo = Ctx->SelectionManager->GetGizmo())
        {
            if (SceneView.bOrthographic)
                Gizmo->ApplyScreenSpaceScalingOrtho(SceneView.CameraOrthoHeight);
            else
                Gizmo->ApplyScreenSpaceScaling(SceneView.CameraPosition);
        }

        Collector.CollectGizmo(Ctx->SelectionManager->GetGizmo(), ShowFlags, Bus, VC->GetViewportState()->bHovered);
        Collector.CollectSelection(Ctx->SelectionManager->GetSelectedActors(), ShowFlags, ViewMode, Bus, bDrawEditorViewportHelpers);
    }

    // 4. CPU 배처 데이터 준비 → GPU 드로우 (SetSubViewport 영역에만 출력됨)
    Renderer.PrepareBatchers(Bus);
    Renderer.Render(Bus);
    Renderer.RenderScreenOverlays(Bus, false);

    TArray<AActor*> IdPickActors;
    Renderer.RenderEditorIdPickBuffer(Bus, ViewportResource, IdPickActors);
    SceneViewport.SetEditorIdPickActors(std::move(IdPickActors));

#if STATS
    const double RenderSec = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - ViewportRenderStart).count();
    static std::chrono::steady_clock::time_point LastPerfLogTimes[FEditorViewportLayout::MaxViewports] = {};
    const auto Now = std::chrono::steady_clock::now();
    const bool bCanLog =
        ViewportIndex >= 0 &&
        ViewportIndex < FEditorViewportLayout::MaxViewports &&
        (LastPerfLogTimes[ViewportIndex].time_since_epoch().count() == 0 ||
            std::chrono::duration<double>(Now - LastPerfLogTimes[ViewportIndex]).count() >= 1.0);
    if (RenderSec >= 0.018 && bCanLog)
    {
        LastPerfLogTimes[ViewportIndex] = Now;
        UE_LOG("[EditorRenderPerf] Viewport=%d Time=%.4fs Opaque=%zu Lights=%zu ShadowRequests=%zu VisiblePrimitives=%d",
               ViewportIndex,
               RenderSec,
               Bus.GetCommands(ERenderPass::Opaque).size(),
               Bus.LightInfos.size(),
               Bus.ShadowLightRequests.size(),
               ViewportCullingStats[ViewportIndex].TotalVisiblePrimitiveCount);
    }
#endif
}

void FEditorRenderPipeline::RenderViewerViewport(FRenderer& Renderer)
{
    TArray<std::unique_ptr<FEditorViewer>>& Viewers = Editor->GetViewers();

	for (size_t i = 0; i < Viewers.size(); i++)
	{
        const auto ViewportRenderStart = std::chrono::steady_clock::now();

        FSceneViewport& SceneViewport = Viewers[i]->GetViewport();
        FEditorViewportClient* VC = SceneViewport.GetClient();

        if (!VC)
            return;

        // 1. SceneView 생성
        FSceneView SceneView;
        VC->BuildSceneView(SceneView);

        const FViewportRect& Rect = SceneViewport.GetRect();
        if (Rect.Width <= 0 || Rect.Height <= 0)
            return;

        // 2. RenderTarget 확보
        FViewportRenderResource& ViewportResource =
            Editor->GetRenderer().AcquireViewerViewportResource(
                (uint32)i,
                Rect.Width,
                Rect.Height);

        SceneViewport.SetRenderTargetSet(&ViewportResource.GetView());

        // 3. Begin
        Renderer.BeginViewportFrame(SceneViewport.GetViewportRenderTargets());

        // 4. Bus 세팅
        Bus.Clear();

        UWorld* World = VC->GetFocusedWorld();
        if (!World)
            return;

        const FEditorSettings& Settings = Editor->GetSettings();
        // Viewer 전용 토글로 글로벌 ShowFlags를 오버라이드.
        // 글로벌 Settings를 mutate하지 않도록 로컬 복사 후 viewer 플래그만 덮어쓴다.
        FShowFlags ShowFlags = Settings.ShowFlags;
        {
            const FSkeletalViewerShowFlags& VFlags = Viewers[i]->GetClient().GetShowFlags();
            ShowFlags.bSkeletalMesh = VFlags.bShowSkeletalMesh;
        }
        const EViewMode ViewMode = SceneView.ViewMode;

        const FViewportCamera* Camera = VC->GetRenderCamera();
        if (!Camera)
            return;

        Bus.SetViewProjection(
            SceneView.ViewMatrix,
            SceneView.ProjectionMatrix,
            Camera->GetNearPlane(),
            Camera->GetFarPlane());

        Bus.SetRenderSettings(ViewMode, ShowFlags);
        Bus.SetLightCullMode(SceneView.LightCullMode);
        Bus.SetShadowFilterMode(Settings.ShadowFilterMode);
        Bus.SetViewportSize(FVector2((float)Rect.Width, (float)Rect.Height));
        Bus.SetViewportOrigin(FVector2(0.0f, 0.0f));
        Bus.SetFXAAEnabled(Settings.bEnableFXAA && !SceneView.bOrthographic);
        Bus.SetCascadeVis(VC->GetViewportState()->bShowCascadeVis);

        // Sandevistan 유지
        if (World->IsSandervistanActivated())
        {
            Bus.bSandevistanEnabled = true;
            Bus.SandevistanIntensity = 1.0f;
        }
        else
        {
            Bus.bSandevistanEnabled = false;
            Bus.SandevistanIntensity = 0.0f;
        }

        // 5. Collect
        const FFrustum& ViewFrustum = SceneView.CameraFrustum;
        const bool bDrawEditorViewportHelpers =
            VC->AllowsEditorWorldControl();

        Collector.CollectWorld(
            World,
            ShowFlags,
            ViewMode,
            Bus,
            &ViewFrustum,
            bDrawEditorViewportHelpers);

        // 🔹 Editor helper 그대로 유지
        if (bDrawEditorViewportHelpers)
        {
            Collector.CollectGrid(
                Settings.GridSpacing,
                Settings.GridHalfLineCount,
                Bus,
                SceneView.bOrthographic);
            const FWorldContext* Ctx = Editor->GetWorldContextFromWorld(World);
            if (UGizmoComponent* Gizmo = Ctx->SelectionManager->GetGizmo())
            {
                if (SceneView.bOrthographic)
                    Gizmo->ApplyScreenSpaceScalingOrtho(SceneView.CameraOrthoHeight);
                else
                    Gizmo->ApplyScreenSpaceScaling(SceneView.CameraPosition);
            }

            Collector.CollectGizmo(
                Ctx->SelectionManager->GetGizmo(),
                ShowFlags,
                Bus,
                VC->GetViewportState()->bHovered);

            Collector.CollectSelection(
                Ctx->SelectionManager->GetSelectedActors(),
                ShowFlags,
                ViewMode,
                Bus,
                bDrawEditorViewportHelpers);
        }

        // Viewer 전용: 본 와이어 드로우 (skeleton mesh viewer 토글)
        if (Viewers[i]->GetClient().GetShowFlags().bShowBones)
        {
            if (ASkeletalMeshActor* ViewTarget = Viewers[i]->GetViewTarget())
            {
                if (USkeletalMeshComponent* SkComp = ViewTarget->GetSkeletalMeshComponent())
                {
                    SkComp->EnsureSkinningUpdated();   // 본 자세 최신화 보장
                    Collector.CollectSkeletonBones(SkComp, Bus);
                }
            }
        }

        // 6. Draw
        Renderer.PrepareBatchers(Bus);
        Renderer.Render(Bus);
        Renderer.RenderScreenOverlays(Bus, false);

        // 7. ID Pick
        TArray<AActor*> IdPickActors;
        Renderer.RenderEditorIdPickBuffer(
            Bus,
            ViewportResource,
            IdPickActors);

        SceneViewport.SetEditorIdPickActors(std::move(IdPickActors));

#if STATS
        const double RenderSec =
            std::chrono::duration<double>(
                std::chrono::steady_clock::now() - ViewportRenderStart)
                .count();

        if (RenderSec >= 0.018)
        {
            UE_LOG("[ViewerRenderPerf] Time=%.4fs", RenderSec);
        }
#endif
	}
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

	if (ViewportIndex < 0 || ViewportIndex >= static_cast<int32>(ViewportDecalStats.size()))
	{
		return EmptyStats;
	}

	return ViewportDecalStats[ViewportIndex];
}

const FRenderCollector::FLightStats& FEditorRenderPipeline::GetViewportLightStats(int32 ViewportIndex) const
{
	static const FRenderCollector::FLightStats EmptyStats{};

	if (ViewportIndex < 0 || ViewportIndex >= static_cast<int32>(ViewportLightStats.size()))
	{
		return EmptyStats;
	}

	return ViewportLightStats[ViewportIndex];
}

ID3D11ShaderResourceView* FEditorRenderPipeline::RenderMaterialPreview(
	FRenderer& Renderer,
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	uint32 Width,
	uint32 Height,
	float YawRad,
	float PitchRad,
	float Distance)
{
	if (Mesh == nullptr || Material == nullptr || !Mesh->HasValidMeshData() || Width == 0 || Height == 0)
	{
		return nullptr;
	}

	FMeshBuffer* MeshBuffer = Collector.GetStaticMeshBuffer(Mesh, 0);
	const FStaticMesh* MeshData = Mesh->GetMeshData(0);
	if (MeshBuffer == nullptr || MeshData == nullptr || MeshData->Indices.empty())
	{
		return nullptr;
	}

	FViewportRenderResource& PreviewTarget = Renderer.AcquirePreviewResource(Width, Height);
	if (!PreviewTarget.GetView().IsValid())
	{
		return nullptr;
	}

	Renderer.BeginViewportFrame(PreviewTarget.GetView());

	Bus.Clear();
	Bus.SetRenderSettings(EViewMode::Lit_BlinnPhong, FShowFlags{});
	Bus.SetLightCullMode(ELightCullMode::None);
	Bus.SetViewportSize(FVector2(static_cast<float>(Width), static_cast<float>(Height)));
	Bus.SetViewportOrigin(FVector2(0.0f, 0.0f));
	Bus.SetFXAAEnabled(true);

	const float ClampedPitch = MathUtil::Clamp(PitchRad, -1.35f, 1.35f);
	const float SafeDistance = std::max(Distance, 0.8f);
	const FVector Eye(SafeDistance, -SafeDistance, SafeDistance * 0.45f);
	const FVector Target = FVector::ZeroVector;
	const FMatrix View = FMatrix::MakeViewLookAtLH(Eye, Target, FVector::UpVector);
	const FMatrix Proj = FMatrix::MakePerspectiveFovLH(MathUtil::DegreesToRadians(45.0f),
		static_cast<float>(Width) / static_cast<float>(Height), 0.05f, 100.0f);
	Bus.SetViewProjection(View, Proj, 0.05f, 100.0f);

	Bus.AmbientLightInfo.Color = FVector(1.0f, 1.0f, 1.0f);
	Bus.AmbientLightInfo.Intensity = 0.25f;
	Bus.DirectionalLightInfo.Color = FVector(1.0f, 0.96f, 0.90f);
	Bus.DirectionalLightInfo.Intensity = 1.5f;
	Bus.DirectionalLightInfo.Direction = FVector(-0.55f, -0.35f, -0.75f).GetSafeNormal();

	const FAABB& Bounds = Mesh->GetLocalBounds();
	const FVector Center = Bounds.GetCenter();
	const FVector Extent = Bounds.GetExtent();
	const float MaxExtent = std::max(0.01f, std::max(Extent.X, std::max(Extent.Y, Extent.Z)));
	const float Scale = 1.35f / MaxExtent;
	const FMatrix CenterToOrigin = FMatrix::MakeTranslationMatrix(FVector(-Center.X, -Center.Y, -Center.Z));
	const FMatrix ScaleMatrix = FMatrix::MakeScaleMatrix(FVector(Scale, Scale, Scale));
	const FMatrix Rotation = FMatrix::MakeRotationY(ClampedPitch) * FMatrix::MakeRotationZ(YawRad);
	const FMatrix World = CenterToOrigin * ScaleMatrix * Rotation;

	if (!MeshData->Sections.empty())
	{
		for (const FStaticMeshSection& Section : MeshData->Sections)
		{
			if (Section.IndexCount == 0)
			{
				continue;
			}

			FRenderCommand Cmd = {};
			Cmd.Type = ERenderCommandType::StaticMesh;
			Cmd.VertexFactoryType = EVertexFactoryType::StaticMesh;
			Cmd.MeshBuffer = MeshBuffer;
			Cmd.Material = Material;
			Cmd.SectionIndexStart = Section.StartIndex;
			Cmd.SectionIndexCount = Section.IndexCount;
			Cmd.PerObjectConstants = FPerObjectConstants(World, FColor::White().ToVector4());
			Cmd.WorldAABB = FAABB::TransformAABB(Bounds, World);
			Bus.AddCommand(ERenderPass::Opaque, Cmd);
		}
	}
	else
	{
		FRenderCommand Cmd = {};
		Cmd.Type = ERenderCommandType::StaticMesh;
		Cmd.VertexFactoryType = EVertexFactoryType::StaticMesh;
		Cmd.MeshBuffer = MeshBuffer;
		Cmd.Material = Material;
		Cmd.SectionIndexStart = 0;
		Cmd.SectionIndexCount = static_cast<uint32>(MeshData->Indices.size());
		Cmd.PerObjectConstants = FPerObjectConstants(World, FColor::White().ToVector4());
		Cmd.WorldAABB = FAABB::TransformAABB(Bounds, World);
		Bus.AddCommand(ERenderPass::Opaque, Cmd);
	}

	Renderer.PrepareBatchers(Bus);
	Renderer.Render(Bus);

	ID3D11ShaderResourceView* PreviewSRV =
		const_cast<ID3D11ShaderResourceView*>(Renderer.GetCurrentSceneSRV());
	Renderer.UseBackBufferRenderTargets();
	return PreviewSRV;
}
