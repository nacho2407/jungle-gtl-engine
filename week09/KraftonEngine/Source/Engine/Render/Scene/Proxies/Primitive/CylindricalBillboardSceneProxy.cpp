#include "Render/Resources/Buffers/ConstantBufferLayouts.h"
#include "Render/Passes/Base/PipelineStateTypes.h"
#include "Render/Scene/Proxies/Primitive/CylindricalBillboardSceneProxy.h"

#include "Component/CylindricalBillboardComponent.h"
#include "GameFramework/AActor.h"
#include "Materials/Material.h"
#include "Materials/MaterialSemantics.h"
#include "Render/Pipelines/Context/Scene/SceneView.h"
#include "Render/Resources/Buffers/MeshBufferManager.h"
#include "Render/Resources/Shaders/ShaderManager.h"
#include "Texture/Texture2D.h"

FCylindricalBillboardSceneProxy::FCylindricalBillboardSceneProxy(UCylindricalBillboardComponent* InComponent)
    : FBillboardSceneProxy(InComponent)
{
    Rasterizer = ERasterizerState::SolidNoCull;
}

void FCylindricalBillboardSceneProxy::UpdateMesh()
{
    FBillboardSceneProxy::UpdateMesh();
}

void FCylindricalBillboardSceneProxy::UpdatePerViewport(const FSceneView& SceneView)
{
    UCylindricalBillboardComponent* Comp = static_cast<UCylindricalBillboardComponent*>(Owner);
    bVisible = Comp->IsVisible();
    if (!bVisible)
    {
        return;
    }

    UMaterial* Mat = Comp->GetMaterial();
    if (Mat)
    {
        for (const char* SlotName : { MaterialSemantics::DiffuseTextureSlot, "BaseColorTexture", "AlbedoTexture", "BaseTexture", "DiffuseMap", "AlbedoMap" })
        {
            UTexture2D* DiffuseTex = nullptr;
            if (Mat->GetTextureParameter(SlotName, DiffuseTex) && DiffuseTex)
            {
                DiffuseSRV = DiffuseTex->GetSRV();
                break;
            }
        }

        MaterialCB[0] = Mat->GetGPUBufferBySlot(ECBSlot::PerShader0);
        MaterialCB[1] = Mat->GetGPUBufferBySlot(ECBSlot::PerShader1);
    }

    if (!Comp->IsBillboardEnabled())
    {
        return;
    }

    const FVector BillboardPos = Comp->GetWorldLocation();
    const FVector BillboardForward = SceneView.CameraForward;

    FVector LocalAxis = Comp->GetBillboardAxis();
    if (LocalAxis.Dot(LocalAxis) < 0.0001f)
    {
        LocalAxis = FVector(0, 0, 1);
    }
    else
    {
        LocalAxis.Normalize();
    }

    FMatrix NonBillboardWorldMatrix;
    if (Comp->GetParent())
    {
        NonBillboardWorldMatrix = Comp->GetRelativeMatrix() * Comp->GetParent()->GetWorldMatrix();
    }
    else
    {
        NonBillboardWorldMatrix = Comp->GetRelativeMatrix();
    }

    const FVector WorldAxis = NonBillboardWorldMatrix.TransformVector(LocalAxis).Normalized();

    FVector Forward = BillboardForward - (WorldAxis * BillboardForward.Dot(WorldAxis));
    if (Forward.Dot(Forward) < 0.0001f)
    {
        const FVector TempUp = (std::abs(WorldAxis.Dot(FVector(0, 0, 1))) < 0.99f) ? FVector(0, 0, 1) : FVector(0, 1, 0);
        Forward = TempUp.Cross(WorldAxis).Normalized();
    }
    else
    {
        Forward.Normalize();
    }

    const FVector Right = WorldAxis.Cross(Forward).Normalized();
    const FVector Up = WorldAxis;

    FMatrix RotMatrix;
    RotMatrix.SetAxes(Forward, Right, Up);

    const FVector WorldScale = Comp->GetWorldScale();
    const FVector SpriteScale(
        (std::abs(WorldScale.X) > 0.0001f) ? WorldScale.X : 0.0001f,
        (std::abs(WorldScale.Y * Comp->GetWidth()) > 0.0001f) ? (WorldScale.Y * Comp->GetWidth()) : 0.0001f,
        (std::abs(WorldScale.Z * Comp->GetHeight()) > 0.0001f) ? (WorldScale.Z * Comp->GetHeight()) : 0.0001f);

    const FMatrix BillboardMatrix = FMatrix::MakeScaleMatrix(SpriteScale) * RotMatrix * FMatrix::MakeTranslationMatrix(BillboardPos);

    PerObjectConstants = FPerObjectConstants::FromWorldMatrix(BillboardMatrix);
    MarkPerObjectCBDirty();
}
