#include "Render/Proxy/GizmoSceneProxy.h"
#include "Component/GizmoComponent.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Resource/ConstantBufferPool.h"
#include "Render/Pipeline/RenderBus.h"

// ============================================================
// FGizmoSceneProxy
// ============================================================
FGizmoSceneProxy::FGizmoSceneProxy(UGizmoComponent* InComponent, bool bInner)
	: FPrimitiveSceneProxy(InComponent)
	, bIsInner(bInner)
{
	bPerViewportUpdate = true;
	Pass = bInner ? ERenderPass::GizmoInner : ERenderPass::GizmoOuter;
}

UGizmoComponent* FGizmoSceneProxy::GetGizmoComponent() const
{
	return static_cast<UGizmoComponent*>(Owner);
}

// ============================================================
// UpdateMesh — 현재 Gizmo 모드에 맞는 메시 버퍼 + 셰이더 캐싱
// ============================================================
void FGizmoSceneProxy::UpdateMesh()
{
	UGizmoComponent* Gizmo = GetGizmoComponent();
	MeshBuffer = Gizmo->GetMeshBuffer();
	Shader = FShaderManager::Get().GetShader(EShaderType::Gizmo);
}

// ============================================================
// UpdateMaterial — ExtraCB(FGizmoConstants) 상태 캐싱
// ============================================================
void FGizmoSceneProxy::UpdateMaterial()
{
	// UpdatePerViewport에서 매 프레임 갱신하므로 여기서는 최소한만
	UGizmoComponent* Gizmo = GetGizmoComponent();

	auto& G = ExtraCB.Bind<FGizmoConstants>(
		FConstantBufferPool::Get().GetBuffer(ECBSlot::Gizmo, sizeof(FGizmoConstants)),
		ECBSlot::Gizmo);

	G.ColorTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	G.bIsInnerGizmo = bIsInner ? 1 : 0;
	G.bClicking = Gizmo->IsHolding() ? 1 : 0;
	G.SelectedAxis = Gizmo->GetSelectedAxis() >= 0
		? static_cast<uint32>(Gizmo->GetSelectedAxis())
		: 0xFFFFFFFFu;
	G.HoveredAxisOpacity = 0.7f;
	G.AxisMask = Gizmo->GetAxisMask();
}

// ============================================================
// UpdatePerViewport — 매 프레임 뷰포트별 스케일 + ExtraCB 갱신
// ============================================================
void FGizmoSceneProxy::UpdatePerViewport(const FRenderBus& Bus)
{
	UGizmoComponent* Gizmo = GetGizmoComponent();

	if (!Bus.GetShowFlags().bGizmo || !Gizmo->IsVisible())
	{
		bVisible = false;
		return;
	}
	bVisible = true;

	// 모드 변경 시 메시가 바뀌므로 매 프레임 갱신
	MeshBuffer = Gizmo->GetMeshBuffer();
	Shader = FShaderManager::Get().GetShader(EShaderType::Gizmo);

	// Per-viewport 스케일 계산
	const FVector CameraPos = Bus.GetView().GetInverseFast().GetLocation();
	float PerViewScale = const_cast<UGizmoComponent*>(Gizmo)->ComputeScreenSpaceScale(
		CameraPos, Bus.IsOrtho(), Bus.GetOrthoWidth());

	FMatrix WorldMatrix = FMatrix::MakeScaleMatrix(FVector(PerViewScale, PerViewScale, PerViewScale))
		* FMatrix::MakeRotationEuler(Gizmo->GetRelativeRotation().ToVector())
		* FMatrix::MakeTranslationMatrix(Gizmo->GetWorldLocation());

	PerObjectConstants = FPerObjectConstants{ WorldMatrix };

	// ExtraCB — FGizmoConstants
	auto& G = ExtraCB.Bind<FGizmoConstants>(
		FConstantBufferPool::Get().GetBuffer(ECBSlot::Gizmo, sizeof(FGizmoConstants)),
		ECBSlot::Gizmo);
	G.ColorTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	G.bIsInnerGizmo = bIsInner ? 1 : 0;
	G.bClicking = Gizmo->IsHolding() ? 1 : 0;
	G.SelectedAxis = Gizmo->GetSelectedAxis() >= 0
		? static_cast<uint32>(Gizmo->GetSelectedAxis())
		: 0xFFFFFFFFu;
	G.HoveredAxisOpacity = 0.7f;
	G.AxisMask = Gizmo->GetAxisMask();
}
