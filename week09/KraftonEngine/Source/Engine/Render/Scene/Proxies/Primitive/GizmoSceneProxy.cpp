#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Scene/Proxies/Primitive/GizmoSceneProxy.h"
#include "Component/GizmoComponent.h"
#include "Render/Resources/Bindings/RenderBindingSlots.h"
#include "Render/Resources/Shaders/ShaderManager.h"
#include "Render/Resources/Buffers/ConstantBufferCache.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Resources/Bindings/RenderCBKeys.h"

// ============================================================
// FGizmoSceneProxy
// ============================================================
FGizmoSceneProxy::FGizmoSceneProxy(UGizmoComponent* InComponent, bool bInner)
    : FPrimitiveSceneProxy(InComponent), bIsInner(bInner)
{
    bPerViewportUpdate = true;
    bNeverCull         = true;
    Pass               = bInner ? ERenderPass::GizmoInner : ERenderPass::GizmoOuter;
}

UGizmoComponent* FGizmoSceneProxy::GetGizmoComponent() const
{
    return static_cast<UGizmoComponent*>(Owner);
}

// ============================================================
// ============================================================
void FGizmoSceneProxy::UpdateMesh()
{
    UGizmoComponent* Gizmo = GetGizmoComponent();
    MeshBuffer             = Gizmo->GetMeshBuffer();
    Shader                 = FShaderManager::Get().GetShader(EShaderType::Gizmo);
}

// ============================================================
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

    MeshBuffer = Gizmo->GetMeshBuffer();
    Shader     = FShaderManager::Get().GetShader(EShaderType::Gizmo);


    const FVector CameraPos    = SceneView.View.GetInverseFast().GetLocation();
    float         PerViewScale = Gizmo->ComputeScreenSpaceScale(
        CameraPos, SceneView.bIsOrtho, SceneView.OrthoWidth);

    FMatrix WorldMatrix = FMatrix::MakeScaleMatrix(FVector(PerViewScale, PerViewScale, PerViewScale)) * FMatrix::MakeRotationEuler(Gizmo->GetRelativeRotation().ToVector()) * FMatrix::MakeTranslationMatrix(Gizmo->GetWorldLocation());

    PerObjectConstants = FPerObjectCBData{ WorldMatrix };
    MarkPerObjectCBDirty();

    // Bind gizmo-specific constants.
    auto& G = ExtraCB.Bind<FGizmoCBData>(
        FConstantBufferCache::Get().GetBuffer(ERenderCBKey::Gizmo, sizeof(FGizmoCBData)),
        ECBSlot::PerShader0);
    G.ColorTint          = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    G.bIsInnerGizmo      = bIsInner ? 1 : 0;
    G.bClicking          = Gizmo->IsHolding() ? 1 : 0;
    G.SelectedAxis       = Gizmo->GetSelectedAxis() >= 0
                               ? static_cast<uint32>(Gizmo->GetSelectedAxis())
                               : 0xFFFFFFFFu;
    G.HoveredAxisOpacity = 0.7f;
    G.AxisMask           = UGizmoComponent::ComputeAxisMask(SceneView.ViewportType, Gizmo->GetMode());
}
