// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Resources/State/RenderStateTypes.h"
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Scene/Proxies/Primitive/DecalSceneProxy.h"

#include "Component/DecalComponent.h"

#include "Materials/Material.h"
#include "Texture/Texture2D.h"
#include "Engine/Runtime/Engine.h"

namespace
{
// FDecalConstants는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FDecalConstants
{
    FMatrix  WorldToDecal;
    FVector4 Color;
};
} // namespace

FDecalSceneProxy::FDecalSceneProxy(UDecalComponent* InComponent)
    : FPrimitiveSceneProxy(InComponent)
{
    DecalCB = new FConstantBuffer();
    if (DecalCB && GEngine)
    {
        DecalCB->Create(GEngine->GetRenderer().GetFD3DDevice().GetDevice(), sizeof(FDecalConstants));
    }

    UpdateMesh();
    UpdateTransform();
}

FDecalSceneProxy::~FDecalSceneProxy()
{
    if (DecalCB)
    {
        DecalCB->Release();
        delete DecalCB;
        DecalCB = nullptr;
    }
}

UDecalComponent* FDecalSceneProxy::GetDecalComponent() const
{
    return static_cast<UDecalComponent*>(Owner);
}

void FDecalSceneProxy::UpdateDecalConstants()
{
    UDecalComponent* DecalComp = GetDecalComponent();
    if (!DecalComp || !DecalCB)
    {
        return;
    }

    auto& CB        = ExtraCB.Bind<FDecalConstants>(DecalCB, ECBSlot::PerShader0);
    CB.WorldToDecal = DecalComp->GetWorldMatrix().GetInverse();
    CB.Color        = DecalComp->GetColor();
}

void FDecalSceneProxy::UpdateTransform()
{
    FPrimitiveSceneProxy::UpdateTransform();
    UpdateDecalConstants();
}

void FDecalSceneProxy::UpdateMaterial()
{
    UDecalComponent* DecalComp = GetDecalComponent();
    if (!DecalComp)
    {
        return;
    }

    DecalMaterial = DecalComp->GetMaterial(0);
    DiffuseSRV    = nullptr;

    if (DecalMaterial)
    {
        UTexture2D* DiffuseTex = nullptr;
        if (DecalMaterial->GetTextureParameter("DiffuseTexture", DiffuseTex))
        {
            DiffuseSRV = DiffuseTex->GetSRV();
        }
    }

    UpdateDecalConstants();
}

void FDecalSceneProxy::UpdateMesh()
{
    UpdateMaterial();

    MeshBuffer = nullptr;
    SectionRenderData.clear();

    Shader           = nullptr;
    Pass             = ERenderPass::Decal;
    Blend            = EBlendState::Opaque;
    DepthStencil     = EDepthStencilState::NoDepth;
    Rasterizer       = ERasterizerState::SolidNoCull;
    bSupportsOutline = false;
}
