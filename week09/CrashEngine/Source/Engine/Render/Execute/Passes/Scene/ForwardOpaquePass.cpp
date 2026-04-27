#include "Render/Execute/Passes/Scene/ForwardOpaquePass.h"

#include "Collision/BVH/DecalVolumeBVH.h"
#include "Component/DecalComponent.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Execute/Passes/Scene/ShadowMapPass.h"
#include "Render/Execute/Registry/RenderPassRegistry.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"
#include "Render/Renderer.h"
#include "Render/Resources/Bindings/RenderBindingSlots.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Scene/Proxies/Primitive/DecalSceneProxy.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Submission/Command/DrawCommandList.h"

namespace
{
    constexpr uint32 MaxForwardDecalTextures = 8;
} // namespace

void FForwardOpaquePass::PrepareInputs(FRenderPipelineContext& Context)
{
    const FViewModePassRegistry* Registry = Context.ViewMode.Registry;
    const bool bUsesLighting = Registry && Registry->UsesLightingPass(Context.ViewMode.ActiveViewMode);

    if (bUsesLighting && Context.Resources)
    {
        // Bind Light Resources to both VS and PS (Forward VS uses them for Gouraud)
        ID3D11Buffer* GlobalLightBuffer = Context.Resources->GlobalLightBuffer.GetBuffer();
        Context.Context->VSSetConstantBuffers(ECBSlot::Light, 1, &GlobalLightBuffer);
        Context.Context->PSSetConstantBuffers(ECBSlot::Light, 1, &GlobalLightBuffer);

        ID3D11ShaderResourceView* LocalLightsSRV = Context.Resources->LocalLightSRV;
        Context.Context->VSSetShaderResources(ESystemTexSlot::LocalLights, 1, &LocalLightsSRV);
        Context.Context->PSSetShaderResources(ESystemTexSlot::LocalLights, 1, &LocalLightsSRV);
    }

    if (bUsesLighting && Context.Renderer)
    {
        if (FRenderPass* Pass = Context.Renderer->GetPassRegistry().FindPass(ERenderPassNodeType::ShadowMapPass))
        {
            FShadowMapPass* ShadowPass = static_cast<FShadowMapPass*>(Pass);
            for (uint32 i = 0; i < ESystemTexSlot::MaxShadowMaps2DCount; ++i)
            {
                ID3D11ShaderResourceView* ShadowSRV2D = ShadowPass->GetShadowSRV2D(i);
                Context.Context->VSSetShaderResources(ESystemTexSlot::ShadowMap2DBase + i, 1, &ShadowSRV2D);
                Context.Context->PSSetShaderResources(ESystemTexSlot::ShadowMap2DBase + i, 1, &ShadowSRV2D);
            }
            for (uint32 i = 0; i < ESystemTexSlot::MaxShadowMapsCubeCount; ++i)
            {
                ID3D11ShaderResourceView* ShadowSRVCube = ShadowPass->GetShadowSRVCube(i);
                Context.Context->VSSetShaderResources(ESystemTexSlot::ShadowMapCubeBase + i, 1, &ShadowSRVCube);
                Context.Context->PSSetShaderResources(ESystemTexSlot::ShadowMapCubeBase + i, 1, &ShadowSRVCube);
            }
        }
    }

    ID3D11ShaderResourceView* ForwardDecalDataSRV = nullptr;
    ID3D11ShaderResourceView* ForwardDecalIndexSRV = nullptr;
    ID3D11ShaderResourceView* ForwardDecalTextureSRVs[MaxForwardDecalTextures] = {};

    if (Context.Submission.SceneData)
    {
        TArray<const FDecalSceneProxy*> ForwardDecals;
        TArray<FForwardDecalGPUData>    ForwardDecalDataList;
        TArray<uint32>                  ForwardDecalIndexList;
        TArray<ID3D11ShaderResourceView*> ForwardDecalTextures;

        for (FPrimitiveProxy* Proxy : Context.Submission.SceneData->Primitives.VisibleProxies)
        {
            if (!Proxy)
            {
                continue;
            }

            Proxy->RelevantForwardDecalIndices.clear();
            Proxy->PerObjectConstants.DecalIndexOffset = 0;
            Proxy->PerObjectConstants.DecalCount = 0;
            Proxy->MarkPerObjectCBDirty();
        }

        for (FPrimitiveProxy* Proxy : Context.Submission.SceneData->Primitives.VisibleProxies)
        {
            if (!Proxy || !Cast<UDecalComponent>(Proxy->Owner) || !Proxy->DiffuseSRV)
            {
                continue;
            }

            int32 TextureSlot = -1;
            for (int32 Index = 0; Index < static_cast<int32>(ForwardDecalTextures.size()); ++Index)
            {
                if (ForwardDecalTextures[Index] == Proxy->DiffuseSRV)
                {
                    TextureSlot = Index;
                    break;
                }
            }

            if (TextureSlot == -1)
            {
                if (ForwardDecalTextures.size() >= MaxForwardDecalTextures)
                {
                    continue;
                }

                TextureSlot = static_cast<int32>(ForwardDecalTextures.size());
                ForwardDecalTextures.push_back(Proxy->DiffuseSRV);
            }

            const FDecalSceneProxy* DecalProxy = static_cast<const FDecalSceneProxy*>(Proxy);
            const FDecalCBData* DecalData = DecalProxy->GetDecalConstants();
            if (!DecalData)
            {
                continue;
            }

            FForwardDecalGPUData& ForwardDecal = ForwardDecalDataList.emplace_back();
            ForwardDecal.WorldToDecal = DecalData->WorldToDecal;
            ForwardDecal.Color = DecalData->Color;
            ForwardDecal.TextureIndex = static_cast<uint32>(TextureSlot);
            ForwardDecals.push_back(DecalProxy);
        }

        FDecalVolumeBVH DecalBVH;
        DecalBVH.Build(ForwardDecals);

        for (FPrimitiveProxy* Proxy : Context.Submission.SceneData->Primitives.OpaqueProxies)
        {
            if (!Proxy || Cast<UDecalComponent>(Proxy->Owner))
            {
                continue;
            }

            DecalBVH.QueryAABB(Proxy->CachedBounds, Proxy->RelevantForwardDecalIndices);
            Proxy->PerObjectConstants.DecalIndexOffset = static_cast<uint32>(ForwardDecalIndexList.size());
            Proxy->PerObjectConstants.DecalCount = static_cast<uint32>(Proxy->RelevantForwardDecalIndices.size());
            Proxy->MarkPerObjectCBDirty();

            for (uint32 DecalIndex : Proxy->RelevantForwardDecalIndices)
            {
                ForwardDecalIndexList.push_back(DecalIndex);
            }

            if (Context.Renderer)
            {
                if (FConstantBuffer* PerObjectCB = Context.Renderer->AcquirePerObjectCBForProxy(*Proxy))
                {
                    PerObjectCB->Update(Context.Context, &Proxy->PerObjectConstants, sizeof(FPerObjectCBData));
                    Proxy->ClearPerObjectCBDirty();
                }
            }
        }

        if (Context.Resources)
        {
            Context.Resources->UpdateForwardDecals(Context.Device->GetDevice(), Context.Context, ForwardDecalDataList, ForwardDecalIndexList);
            ForwardDecalDataSRV = Context.Resources->ForwardDecalDataSRV;
            ForwardDecalIndexSRV = Context.Resources->ForwardDecalIndexSRV;
        }

        for (uint32 TextureIndex = 0; TextureIndex < static_cast<uint32>(ForwardDecalTextures.size()) && TextureIndex < MaxForwardDecalTextures; ++TextureIndex)
        {
            ForwardDecalTextureSRVs[TextureIndex] = ForwardDecalTextures[TextureIndex];
        }
    }

    Context.Context->PSSetShaderResources(ESystemTexSlot::ForwardDecalData, 1, &ForwardDecalDataSRV);
    Context.Context->PSSetShaderResources(ESystemTexSlot::ForwardDecalIndexList, 1, &ForwardDecalIndexSRV);
    Context.Context->PSSetShaderResources(ESystemTexSlot::ForwardDecalTextureBase, MaxForwardDecalTextures, ForwardDecalTextureSRVs);

    if (Context.StateCache)
    {
        Context.StateCache->bForceAll = true;
    }
}

void FForwardOpaquePass::PrepareTargets(FRenderPipelineContext& Context)
{
    ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
    Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
}

void FForwardOpaquePass::BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy)
{
    DrawCommandBuild::BuildMeshDrawCommand(Proxy, ERenderPass::Opaque, Context, *Context.DrawCommandList);
}

void FForwardOpaquePass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    SubmitPassRange(Context, ERenderPass::Opaque);
}

void FForwardOpaquePass::Cleanup(FRenderPipelineContext& Context)
{
    ID3D11ShaderResourceView* NullSRV = nullptr;
    for (uint32 i = 0; i < ESystemTexSlot::MaxShadowMaps2DCount; ++i)
    {
        Context.Context->VSSetShaderResources(ESystemTexSlot::ShadowMap2DBase + i, 1, &NullSRV);
        Context.Context->PSSetShaderResources(ESystemTexSlot::ShadowMap2DBase + i, 1, &NullSRV);
    }
    for (uint32 i = 0; i < ESystemTexSlot::MaxShadowMapsCubeCount; ++i)
    {
        Context.Context->VSSetShaderResources(ESystemTexSlot::ShadowMapCubeBase + i, 1, &NullSRV);
        Context.Context->PSSetShaderResources(ESystemTexSlot::ShadowMapCubeBase + i, 1, &NullSRV);
    }

    Context.Context->PSSetShaderResources(ESystemTexSlot::ForwardDecalData, 1, &NullSRV);
    Context.Context->PSSetShaderResources(ESystemTexSlot::ForwardDecalIndexList, 1, &NullSRV);

    ID3D11ShaderResourceView* NullDecalTextureSRVs[MaxForwardDecalTextures] = {};
    Context.Context->PSSetShaderResources(ESystemTexSlot::ForwardDecalTextureBase, MaxForwardDecalTextures, NullDecalTextureSRVs);
}
