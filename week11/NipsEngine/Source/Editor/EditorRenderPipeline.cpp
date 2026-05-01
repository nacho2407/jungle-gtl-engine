#include "EditorRenderPipeline.h"

#include "Editor/EditorEngine.h"
#include "Camera/ViewportCamera.h"
#include "Render/Renderer/Renderer.h"
#include "GameFramework/World.h"
#include "Core/Logging/Stats.h"
#include "Core/Logging/GPUProfiler.h"
#include "Runtime/SceneView.h"
#include "Engine/Component/GizmoComponent.h"
#include "Asset/StaticMesh.h"
#include "Render/Resource/Buffer.h"
#include "Render/Resource/Material.h"
#include "Render/Scene/RenderCommand.h"
#include "Math/Utils.h"

#include <algorithm>

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
#if STATS
    FStatManager::Get().TakeSnapshot();
    FGPUProfiler::Get().TakeSnapshot();
#endif

    for (FRenderCollector::FCullingStats& Stats : ViewportCullingStats)
    {
        Stats = {};
    }

    if (!Editor->GetFocusedWorld())
        return;

    // 1회: 전체 백버퍼 클리어 (색상 + 깊이/스텐실)
    Renderer.BeginFrame();

    // 4개 뷰포트를 순서대로 렌더링
    for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
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

    FSceneViewport& SceneViewport = Editor->GetViewportLayout().GetSceneViewport(ViewportIndex);
    
	// Width, Height 변경 여부에 따라 Resource 버퍼 재생성
	// 만약 최소화 등의 상황으로 (H, W) == (0, 0) 일 경우 Render 안함
	FViewportRenderResource& ViewportResource = Editor->GetRenderer().AcquireViewportResource(&SceneViewport, Rect.Width, Rect.Height, ViewportIndex);
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
	Bus.SetViewportSize(FVector2(static_cast<float>(Rect.Width), static_cast<float>(Rect.Height)));
    Bus.SetViewportOrigin(FVector2(0.0f, 0.0f));
    Bus.SetFXAAEnabled(Settings.bEnableFXAA && !SceneView.bOrthographic);
    Bus.SetCascadeVis(VC->GetViewportState()->bShowCascadeVis);

    const FFrustum& ViewFrustum = SceneView.CameraFrustum;
    Collector.CollectWorld(World, ShowFlags, ViewMode, Bus, &ViewFrustum);
    ViewportCullingStats[ViewportIndex] = Collector.GetLastCullingStats();
    ViewportDecalStats[ViewportIndex] = Collector.GetLastDecalStats();
    ViewportLightStats[ViewportIndex] = Collector.GetLastLightStats();
    Collector.CollectGrid(Settings.GridSpacing, Settings.GridHalfLineCount, Bus, SceneView.bOrthographic);

    // Editor controller가 활성화된 뷰포트에서 기즈모·선택 오버레이를 그립니다.
    // PIE Eject 상태도 PIE World를 대상으로 편집 컨트롤을 사용할 수 있어야 합니다.
    if (VC->AllowsEditorWorldControl())
    {
        if (UGizmoComponent* Gizmo = Editor->GetGizmo())
        {
            if (SceneView.bOrthographic)
                Gizmo->ApplyScreenSpaceScalingOrtho(SceneView.CameraOrthoHeight);
            else
                Gizmo->ApplyScreenSpaceScaling(SceneView.CameraPosition);
        }

        Collector.CollectGizmo(Editor->GetGizmo(), ShowFlags, Bus, VC->GetViewportState()->bHovered);
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
