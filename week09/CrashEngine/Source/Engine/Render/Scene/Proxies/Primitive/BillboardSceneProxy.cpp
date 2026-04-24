// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Resources/State/RenderStateTypes.h"
#include "Render/Resources/Meshes/PrimitiveMeshTypes.h"
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Scene/Proxies/Primitive/BillboardSceneProxy.h"
#include "Component/BillboardComponent.h"
#include "Render/Resources/Shaders/ShaderManager.h"
#include "Render/Resources/Buffers/MeshBufferManager.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "GameFramework/AActor.h"
#include "Materials/Material.h"
#include "Materials/MaterialSemantics.h"
#include "Render/Resources/Bindings/RenderBindingSlots.h"
#include "Texture/Texture2D.h"
#include <algorithm>

// ============================================================
// FBillboardSceneProxy
// ============================================================
FBillboardSceneProxy::FBillboardSceneProxy(UBillboardComponent* InComponent)
    : FPrimitiveSceneProxy(InComponent)
{
    bPerViewportUpdate           = true;
    bShowAABB                    = false;
    bAllowViewModeShaderOverride = false;
}

UBillboardComponent* FBillboardSceneProxy::GetBillboardComponent() const
{
    return static_cast<UBillboardComponent*>(Owner);
}

namespace
{
ID3D11ShaderResourceView* ResolveBillboardTextureSRV(UMaterial* Material)
{
    if (!Material)
    {
        return nullptr;
    }

    for (const char* SlotName : { MaterialSemantics::DiffuseTextureSlot, "BaseColorTexture", "AlbedoTexture", "BaseTexture", "DiffuseMap", "AlbedoMap" })
    {
        UTexture2D* DiffuseTex = nullptr;

        if (Material->GetTextureParameter(SlotName, DiffuseTex) && DiffuseTex)
        {
            if (ID3D11ShaderResourceView* SRV = DiffuseTex->GetSRV())
            {
                return SRV;
            }
        }
    }

    return nullptr;
}
} // namespace

#include "Editor/UI/EditorConsolePanel.h"

// ============================================================
// ============================================================
void FBillboardSceneProxy::UpdateMesh()
{
    UBillboardComponent* Comp = GetBillboardComponent();
    UMaterial*           Mat  = Comp ? Comp->GetMaterial() : nullptr;

    MeshBuffer   = &FMeshBufferManager::Get().GetMeshBuffer(EMeshShape::TexturedQuad);
    Shader       = FShaderManager::Get().GetShader(EShaderType::Billboard);
    Pass         = ERenderPass::AlphaBlend;
    Blend        = EBlendState::AlphaBlend;
    DepthStencil = EDepthStencilState::DepthReadOnly;
    Rasterizer   = ERasterizerState::SolidNoCull;

    DiffuseSRV    = nullptr;
    NormalSRV     = nullptr;
    SpecularSRV   = nullptr;
    MaterialCB[0] = nullptr;
    MaterialCB[1] = nullptr;

    if (Mat)
    {
        DiffuseSRV    = ResolveBillboardTextureSRV(Mat);
        MaterialCB[0] = Mat->GetGPUBufferBySlot(ECBSlot::PerShader0);
        MaterialCB[1] = Mat->GetGPUBufferBySlot(ECBSlot::PerShader1);
    }

    if (Mat && !DiffuseSRV)
    {
        UE_LOG("Billboard DiffuseSRV is null. Material=%s", Mat->GetAssetPathFileName().c_str());
    }
}

// ============================================================
// ============================================================
void FBillboardSceneProxy::UpdatePerViewport(const FSceneView& SceneView)
{
    UBillboardComponent* Comp = GetBillboardComponent();
    bVisible                  = Comp->ShouldRenderInCurrentWorld();
    if (!bVisible)
        return;

    UMaterial* Mat = Comp->GetMaterial();
    if (Mat)
    {
        DiffuseSRV    = ResolveBillboardTextureSRV(Mat);
        MaterialCB[0] = Mat->GetGPUBufferBySlot(ECBSlot::PerShader0);
        MaterialCB[1] = Mat->GetGPUBufferBySlot(ECBSlot::PerShader1);
    }

    const FVector WorldScale = Comp->GetWorldScale();
    const FVector SpriteScale(std::max(WorldScale.X, 1.0f), Comp->GetWidth() * WorldScale.Y, Comp->GetHeight() * WorldScale.Z);

    FMatrix WorldMatrix;
    if (Comp->IsBillboardEnabled())
    {
        WorldMatrix = Comp->ComputeBillboardMatrix(SceneView.CameraForward);
    }
    else
    {
        WorldMatrix = FMatrix::MakeScaleMatrix(SpriteScale) * Comp->GetRelativeMatrix() * FMatrix::MakeTranslationMatrix(Comp->GetWorldLocation());
    }

    PerObjectConstants = FPerObjectCBData::FromWorldMatrix(WorldMatrix);
    MarkPerObjectCBDirty();
}
