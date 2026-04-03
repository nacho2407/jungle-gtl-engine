#include "Render/Pipeline/GizmoSceneProxy.h"
#include "Component/GizmoComponent.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Resource/ConstantBufferPool.h"

// ============================================================
// FGizmoSceneProxy
// ============================================================
FGizmoSceneProxy::FGizmoSceneProxy(UGizmoComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	bPerViewportUpdate = true;
	Pass = ERenderPass::GizmoOuter; // 기본 패스 (실제 렌더 시 Outer/Inner 둘 다 사용)
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
	UGizmoComponent* Gizmo = GetGizmoComponent();

	auto& G = ExtraCB.Bind<FGizmoConstants>(
		FConstantBufferPool::Get().GetBuffer(ECBSlot::Gizmo, sizeof(FGizmoConstants)),
		ECBSlot::Gizmo);

	G.ColorTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	G.bIsInnerGizmo = 0;
	G.bClicking = Gizmo->IsHolding() ? 1 : 0;
	G.SelectedAxis = Gizmo->GetSelectedAxis() >= 0
		? static_cast<uint32>(Gizmo->GetSelectedAxis())
		: 0xFFFFFFFFu;
	G.HoveredAxisOpacity = 0.7f;
	G.AxisMask = Gizmo->GetAxisMask();
}
