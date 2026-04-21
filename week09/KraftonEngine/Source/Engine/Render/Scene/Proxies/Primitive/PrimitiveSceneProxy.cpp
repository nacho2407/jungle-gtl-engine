#include "Render/Resources/Buffers/ConstantBufferLayouts.h"
#include "Render/Passes/Base/RenderPassTypes.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Scene/Scene.h"
#include "Component/PrimitiveComponent.h"
#include "Component/ActorComponent.h"
#include "GameFramework/AActor.h"
#include "Render/Resources/Shaders/ShaderManager.h"

// ============================================================
// FPrimitiveSceneProxy — 기본 구현
// ============================================================
FPrimitiveSceneProxy::FPrimitiveSceneProxy(UPrimitiveComponent* InComponent)
    : Owner(InComponent)
{
    bSupportsOutline = Owner->SupportsOutline();
}

void FPrimitiveSceneProxy::UpdateTransform()
{
    PerObjectConstants = FPerObjectConstants::FromWorldMatrix(Owner->GetWorldMatrix());
    CachedWorldPos = PerObjectConstants.Model.GetLocation();
    CachedBounds = Owner->GetWorldBoundingBox();
    LastLODUpdateFrame = UINT32_MAX;
    MarkPerObjectCBDirty();
}

void FPrimitiveSceneProxy::UpdateMaterial()
{
    // 기본 PrimitiveComponent는 섹션별 머티리얼이 없음 — 서브클래스에서 오버라이드
}

void FPrimitiveSceneProxy::UpdateVisibility()
{
    bVisible = Owner->IsVisible();
    if (bVisible)
    {
        AActor* OwnerActor = Owner->GetOwner();
        if (OwnerActor && !OwnerActor->IsVisible())
            bVisible = false;
    }
}

void FPrimitiveSceneProxy::UpdateMesh()
{
    MeshBuffer = Owner->GetMeshBuffer();
    Shader = FShaderManager::Get().GetShader(EShaderType::Primitive);
    Pass = ERenderPass::Opaque;
}

void FPrimitiveSceneProxy::CollectSelectedVisuals(FScene& Scene) const
{
    if (!Owner)
        return;
    AActor* Actor = Owner->GetOwner();
    if (!Actor)
        return;

    for (UActorComponent* Comp : Actor->GetComponents())
    {
        if (Comp)
            Comp->ContributeSelectedVisuals(Scene);
    }
}
