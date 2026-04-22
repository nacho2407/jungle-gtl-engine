#include "Render/Resources/Buffers/ConstantBufferLayouts.h"
#include "Render/Execute/Passes/Base/RenderPassTypes.h"
#include "Render/Scene/Proxies/Primitive/GizmoSceneProxy.h"
#include "Component/GizmoComponent.h"
#include "Render/Resources/Shaders/ShaderManager.h"
#include "Render/Resources/Buffers/ConstantBufferPool.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Resources/Bindings/RenderCBKeys.h"

// ============================================================
// FGizmoSceneProxy
// ============================================================
FGizmoSceneProxy::FGizmoSceneProxy(UGizmoComponent* InComponent, bool bInner)
    : FPrimitiveSceneProxy(InComponent), bIsInner(bInner)
{
    bPerViewportUpdate = true;
    bNeverCull = true;
    Pass = bInner ? ERenderPass::GizmoInner : ERenderPass::GizmoOuter;
}

UGizmoComponent* FGizmoSceneProxy::GetGizmoComponent() const
{
    return static_cast<UGizmoComponent*>(Owner);
}

// ============================================================
// UpdateMesh ???�재 Gizmo 모드??맞는 메시 버퍼 + ?�이??캐싱
// ============================================================
void FGizmoSceneProxy::UpdateMesh()
{
    UGizmoComponent* Gizmo = GetGizmoComponent();
    MeshBuffer = Gizmo->GetMeshBuffer();
    Shader = FShaderManager::Get().GetShader(EShaderType::Gizmo);
}

// ============================================================
// UpdatePerViewport ??�??�레??뷰포?�별 ?��???+ ExtraCB 갱신
// ============================================================
void FGizmoSceneProxy::UpdatePerViewport(const FSceneView& SceneView)
{
    UGizmoComponent* Gizmo = GetGizmoComponent();

    if (!SceneView.ShowFlags.bGizmo || !Gizmo->IsVisible())
    {
        bVisible = false;
        return;
    }
    bVisible = true;

    // 모드 변�???메시가 바뀌�?�?�??�레??갱신
    MeshBuffer = Gizmo->GetMeshBuffer();
    Shader = FShaderManager::Get().GetShader(EShaderType::Gizmo);


    // Per-viewport ?��???계산
    const FVector CameraPos = SceneView.View.GetInverseFast().GetLocation();
    float PerViewScale = Gizmo->ComputeScreenSpaceScale(
        CameraPos, SceneView.bIsOrtho, SceneView.OrthoWidth);

    FMatrix WorldMatrix = FMatrix::MakeScaleMatrix(FVector(PerViewScale, PerViewScale, PerViewScale)) * FMatrix::MakeRotationEuler(Gizmo->GetRelativeRotation().ToVector()) * FMatrix::MakeTranslationMatrix(Gizmo->GetWorldLocation());

    PerObjectConstants = FPerObjectConstants{ WorldMatrix };
    MarkPerObjectCBDirty();

    // ExtraCB ??FGizmoConstants
    auto& G = ExtraCB.Bind<FGizmoConstants>(
        FConstantBufferPool::Get().GetBuffer(ERenderCBKey::Gizmo, sizeof(FGizmoConstants)),
        ECBSlot::PerShader0);
    G.ColorTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    G.bIsInnerGizmo = bIsInner ? 1 : 0;
    G.bClicking = Gizmo->IsHolding() ? 1 : 0;
    G.SelectedAxis = Gizmo->GetSelectedAxis() >= 0
                         ? static_cast<uint32>(Gizmo->GetSelectedAxis())
                         : 0xFFFFFFFFu;
    G.HoveredAxisOpacity = 0.7f;
    G.AxisMask = UGizmoComponent::ComputeAxisMask(SceneView.ViewportType, Gizmo->GetMode());
}
