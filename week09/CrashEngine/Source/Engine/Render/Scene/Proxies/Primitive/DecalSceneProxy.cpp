// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Resources/State/RenderStateTypes.h"
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Scene/Proxies/Primitive/DecalSceneProxy.h"

#include "Component/DecalComponent.h"

#include "Materials/Material.h"
#include "Texture/Texture2D.h"
#include "Engine/Runtime/Engine.h"

FDecalSceneProxy::FDecalSceneProxy(UDecalComponent* InComponent)
    : FPrimitiveProxy(InComponent)
{
    DecalCB = new FConstantBuffer();
    if (DecalCB && GEngine)
    {
        DecalCB->Create(GEngine->GetRenderer().GetFD3DDevice().GetDevice(), sizeof(FDecalCBData));
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

    auto& CB        = ExtraCB.Bind<FDecalCBData>(DecalCB, ECBSlot::PerShader0);
    CB.WorldToDecal = DecalComp->GetWorldMatrix().GetInverse();
    CB.Color        = DecalComp->GetColor();
}

void FDecalSceneProxy::UpdateTransform()
{
    FPrimitiveProxy::UpdateTransform();
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

