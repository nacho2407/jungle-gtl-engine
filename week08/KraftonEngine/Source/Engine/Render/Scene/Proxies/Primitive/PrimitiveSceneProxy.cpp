// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Scene/Scene.h"
#include "Component/PrimitiveComponent.h"
#include "Component/ActorComponent.h"
#include "GameFramework/AActor.h"
#include "Render/Resources/Shaders/ShaderManager.h"

// ============================================================
// ============================================================
FPrimitiveSceneProxy::FPrimitiveSceneProxy(UPrimitiveComponent* InComponent)
    : Owner(InComponent)
{
    bSupportsOutline = Owner->SupportsOutline();
}

void FPrimitiveSceneProxy::UpdateTransform()
{
    PerObjectConstants = FPerObjectCBData::FromWorldMatrix(Owner->GetWorldMatrix());
    CachedWorldPos     = PerObjectConstants.Model.GetLocation();
    CachedBounds       = Owner->GetWorldBoundingBox();
    LastLODUpdateFrame = UINT32_MAX;
    MarkPerObjectCBDirty();
}

void FPrimitiveSceneProxy::UpdateMaterial()
{
}

void FPrimitiveSceneProxy::UpdateVisibility()
{
    if (!Owner)
    {
        bVisible = false;
        return;
    }

    bVisible = Owner->ShouldRenderInCurrentWorld();
    if (!bVisible)
    {
        return;
    }

    AActor* OwnerActor = Owner->GetOwner();
    if (OwnerActor && !OwnerActor->IsVisible())
    {
        bVisible = false;
    }
}

void FPrimitiveSceneProxy::UpdateMesh()
{
    MeshBuffer = Owner->GetMeshBuffer();
    Shader     = FShaderManager::Get().GetShader(EShaderType::Primitive);
    Pass       = ERenderPass::Opaque;
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
