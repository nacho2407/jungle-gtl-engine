// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
#include "Render/Scene/Scene.h"
#include "Component/PrimitiveComponent.h"
#include "Component/ActorComponent.h"
#include "GameFramework/AActor.h"
#include "Render/Resources/Shaders/ShaderManager.h"

// ============================================================
// ============================================================
FPrimitiveProxy::FPrimitiveProxy(UPrimitiveComponent* InComponent)
    : Owner(InComponent)
{
    bSupportsOutline = Owner->SupportsOutline();
}

void FPrimitiveProxy::UpdateTransform()
{
    PerObjectConstants = FPerObjectCBData::FromWorldMatrix(Owner->GetWorldMatrix());
    CachedWorldPos     = PerObjectConstants.Model.GetLocation();
    CachedBounds       = Owner->GetWorldBoundingBox();
    LastLODUpdateFrame = UINT32_MAX;
    MarkPerObjectCBDirty();
}

void FPrimitiveProxy::UpdateMaterial()
{
}

void FPrimitiveProxy::UpdateVisibility()
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

void FPrimitiveProxy::UpdateMesh()
{
    MeshBuffer = Owner->GetMeshBuffer();
    Shader     = FShaderManager::Get().GetShader(EShaderType::Primitive);
    Pass       = ERenderPass::Opaque;
}

void FPrimitiveProxy::CollectSelectedVisuals(FScene& Scene) const
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

