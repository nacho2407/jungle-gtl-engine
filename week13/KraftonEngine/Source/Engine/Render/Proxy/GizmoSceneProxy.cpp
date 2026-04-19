#include "Render/Proxy/GizmoSceneProxy.h"
#include "Component/GizmoComponent.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Resource/ConstantBufferPool.h"
#include "Render/Pipeline/FrameContext.h"

// ============================================================
// FGizmoSceneProxy
// ============================================================
FGizmoSceneProxy::FGizmoSceneProxy(UGizmoComponent* InComponent, bool bInner)
	: FPrimitiveSceneProxy(InComponent)
	, bIsInner(bInner)
{
	ProxyFlags |= EPrimitiveProxyFlags::PerViewportUpdate
	            | EPrimitiveProxyFlags::NeverCull;
	ProxyFlags &= ~(EPrimitiveProxyFlags::SupportsOutline
	              | EPrimitiveProxyFlags::ShowAABB);
	Pass = bInner ? ERenderPass::GizmoInner : ERenderPass::GizmoOuter;
}

UGizmoComponent* FGizmoSceneProxy::GetGizmoComponent() const
{
	return static_cast<UGizmoComponent*>(GetOwner());
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
// UpdatePerViewport — 매 프레임 뷰포트별 스케일 + ExtraCB 갱신
// ============================================================
void FGizmoSceneProxy::UpdatePerViewport(const FFrameContext& Frame)
{
	UGizmoComponent* Gizmo = GetGizmoComponent();

	if (!Frame.RenderOptions.ShowFlags.bGizmo || !Gizmo->IsVisible())
	{
		bVisible = false;
		return;
	}
	bVisible = true;

	// 모드 변경 시 메시가 바뀌므로 매 프레임 갱신
	MeshBuffer = Gizmo->GetMeshBuffer();
	Shader = FShaderManager::Get().GetShader(EShaderType::Gizmo);


	// Per-viewport 스케일 계산
	const FVector CameraPos = Frame.View.GetInverseFast().GetLocation();
	float PerViewScale = Gizmo->ComputeScreenSpaceScale(
		CameraPos, Frame.bIsOrtho, Frame.OrthoWidth);

	FMatrix WorldMatrix = FMatrix::MakeScaleMatrix(FVector(PerViewScale, PerViewScale, PerViewScale))
		* FMatrix::MakeRotationEuler(Gizmo->GetRelativeRotation().ToVector())
		* FMatrix::MakeTranslationMatrix(Gizmo->GetWorldLocation());

	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(WorldMatrix);
	MarkPerObjectCBDirty();

	// ExtraCB — FGizmoConstants
	auto& G = ExtraCB.Bind<FGizmoConstants>(
		FConstantBufferPool::Get().GetBuffer(ECBPoolKey::Gizmo, sizeof(FGizmoConstants)),
		ECBSlot::PerShader0);
	G.ColorTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	G.bIsInnerGizmo = bIsInner ? 1 : 0;
	G.bClicking = Gizmo->IsHolding() ? 1 : 0;
	G.SelectedAxis = Gizmo->GetSelectedAxis() >= 0
		? static_cast<uint32>(Gizmo->GetSelectedAxis())
		: 0xFFFFFFFFu;
	G.HoveredAxisOpacity = 0.7f;
	G.AxisMask = UGizmoComponent::ComputeAxisMask(Frame.RenderOptions.ViewportType, Gizmo->GetMode());
}
