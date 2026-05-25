#include "DecalRenderPass.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/Material.h"
#include "Render/Resource/ShadowAtlasManager.h"
#include "Render/Resource/ShaderHelper.h"
#include "Render/Resource/VertexFactoryTypes.h"
#include "Core/ResourceManager.h"
#include "Component/PostProcess/Light/LightComponent.h"

namespace
{
    bool ShouldSkipDecalPass(EViewMode ViewMode)
    {
        switch (ViewMode)
        {
        case EViewMode::Lit_Gouraud:
        case EViewMode::Lit_Lambert:
        case EViewMode::Lit_BlinnPhong:
            return false;
        default:
            return true;
        }
    }

    FShaderProgram* GetShaderProgram(const FRenderCommand& Cmd, uint32 PermutationKey)
    {
        if (!Cmd.Material)
        {
            return nullptr;
        }
        if (Cmd.Material->GetShaderType() == EMaterialShaderType::None)
        {
            UE_LOG_WARNING("[Render] ShaderType None material cannot be drawn by DecalRenderPass: %s", Cmd.Material->GetName().c_str());
            return nullptr;
        }

        const FVertexFactoryDesc& VertexFactoryDesc = FVertexFactoryRegistry::Get(Cmd.VertexFactoryType);

        FShaderStageKey VSKey;
        VSKey.FilePath = VertexFactoryDesc.VertexShaderPath;
        VSKey.EntryPoint = VertexFactoryDesc.BasePassVSEntry;
        VSKey.Target = "vs_5_0";
        VSKey.PermutationKey = PermutationKey;

        FShaderStageKey PSKey;
        PSKey.FilePath = Cmd.Material->GetPixelShaderPath();
        PSKey.EntryPoint = Cmd.Material->GetPixelShaderEntryPoint();
        PSKey.Target = "ps_5_0";
        PSKey.PermutationKey = PermutationKey;

        TArray<D3D_SHADER_MACRO> Macros = FShaderHelper::BuildUberLitMacros(PermutationKey);
        return FResourceManager::Get().GetOrCreateShaderProgram(
            VSKey,
            PSKey,
            Macros.data(),
            Macros.data(),
            &VertexFactoryDesc.VertexLayout);
    }

    void BindLightingResources(const FRenderPassContext* Context)
    {
        ID3D11SamplerState* ShadowSampler = FResourceManager::Get().GetOrCreateSamplerState(ESamplerType::EST_Shadow);
        Context->DeviceContext->PSSetSamplers(1, 1, &ShadowSampler);

        ID3D11ShaderResourceView* ShadowSRV = FShadowAtlasManager::Get().GetSRV();
        Context->DeviceContext->PSSetShaderResources(10, 1, &ShadowSRV);

        ID3D11ShaderResourceView* VSMSRV = FShadowAtlasManager::Get().GetVarianceSRV();
        Context->DeviceContext->PSSetShaderResources(11, 1, &VSMSRV);

        ID3D11ShaderResourceView* PointShadowCubeSRV = FShadowAtlasManager::Get().GetCubeSRV();
        Context->DeviceContext->PSSetShaderResources(12, 1, &PointShadowCubeSRV);

        ID3D11ShaderResourceView* ShadowInfoSRVs[] = {
            Context->RenderResources->LightShadowIndexBuffer.GetSRV(),
            Context->RenderResources->AtlasShadowBuffer.GetSRV(),
        };
        Context->DeviceContext->PSSetShaderResources(14, 2, ShadowInfoSRVs);
    }
} // namespace

bool FDecalRenderPass::Initialize()
{
    return true;
}

bool FDecalRenderPass::Release()
{
    return true;
}

bool FDecalRenderPass::Begin(const FRenderPassContext* Context)
{
    const TArray<FRenderCommand>& Commands = Context->RenderBus->GetCommands(ERenderPass::Decal);
    if (Commands.empty() || ShouldSkipDecalPass(Context->RenderBus->GetViewMode()))
    {
        OutSRV = PrevPassSRV;
        OutRTV = PrevPassRTV;
        return true;
    }

    const FRenderTargetSet* RenderTargets = Context->RenderTargets;
    ID3D11RenderTargetView* RTV = RenderTargets->SceneColorRTV;
    ID3D11DepthStencilView* DSV = RenderTargets->DepthStencilView;

    Context->DeviceContext->OMSetRenderTargets(1, &RTV, DSV);
    OutSRV = RenderTargets->SceneColorSRV;
    OutRTV = RenderTargets->SceneColorRTV;

    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    BindLightingResources(Context);
    return true;
}

bool FDecalRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    const FRenderBus* RenderBus = Context->RenderBus;
    const TArray<FRenderCommand>& Commands = RenderBus->GetCommands(ERenderPass::Decal);

    if (Commands.empty() || ShouldSkipDecalPass(RenderBus->GetViewMode()))
    {
        return true;
    }

    for (const FRenderCommand& Cmd : Commands)
    {
        if (!DrawEachCommand(Context, Cmd)) 
            continue;        // Draw 실패 시 동작 추가할 거면 여기에 추가
    }

    return true;
}

bool FDecalRenderPass::DrawEachCommand(const FRenderPassContext* Context, const FRenderCommand& Cmd)
{
    Context->RenderResources->PerObjectConstantBuffer.Update(Context->DeviceContext, &Cmd.PerObjectConstants, sizeof(FPerObjectConstants));
    ID3D11Buffer* cb1 = Context->RenderResources->PerObjectConstantBuffer.GetBuffer();
    Context->DeviceContext->VSSetConstantBuffers(1, 1, &cb1);
    Context->DeviceContext->PSSetConstantBuffers(1, 1, &cb1);

    if (Cmd.MeshBuffer == nullptr || !Cmd.MeshBuffer->IsValid())
    {
        return false;
    }

    uint32 offset = 0;
    ID3D11Buffer* vertexBuffer = Cmd.MeshBuffer->GetVertexBuffer().GetBuffer();
    if (vertexBuffer == nullptr)
    {
        return false;
    }

    uint32 vertexCount = Cmd.MeshBuffer->GetVertexBuffer().GetVertexCount();
    uint32 stride = Cmd.MeshBuffer->GetVertexBuffer().GetStride();
    if (vertexCount == 0 || stride == 0)
    {
        return false;
    }

    if (Cmd.Material)
    {
        uint32 PermutationKey = 0;
        PermutationKey |= GetLightingModelPermutationKey(Context->RenderBus->GetViewMode());
        PermutationKey |= GetLightCullingPermutationKey(Context);
        PermutationKey |= GetShadowMapPermutationKey(Context);
        PermutationKey |= GetTexturePermutationKey(Cmd.Material, false, true, false, false);
            
        FShaderProgram* Program = GetShaderProgram(Cmd, PermutationKey);
        if (!Program)
        {
            return false;
        }

        Program->Bind(Context->DeviceContext);
        Cmd.Material->BindRenderStates(Context->DeviceContext);
        Cmd.Material->BindParameters(Context->DeviceContext, Program->PS);

        Context->RenderResources->ProjectionDecalConstantBuffer.Update(
            Context->DeviceContext,
            &Cmd.Constants.Decal,
            sizeof(FProjectionDecalConstants));
        ID3D11Buffer* DecalBuffer = Context->RenderResources->ProjectionDecalConstantBuffer.GetBuffer();
        Context->DeviceContext->PSSetConstantBuffers(8, 1, &DecalBuffer);

        BindVertexFactoryResources(
            Context->DeviceContext,
            Cmd.VertexFactoryType,
            Context->RenderBus->GetBoneMatrixConstants(Cmd),
            Context->RenderResources,
            Cmd.BoneMatrixConstantBuffer);
    }
    CheckOverrideViewMode(Context);  
    Context->DeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

    ID3D11Buffer* indexBuffer = Cmd.MeshBuffer->GetIndexBuffer().GetBuffer();
    if (indexBuffer != nullptr)
    {
        uint32 indexStart = Cmd.SectionIndexStart;
        uint32 indexCount = Cmd.SectionIndexCount;
        Context->DeviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        Context->DeviceContext->DrawIndexed(indexCount, indexStart, 0);
    }
    else
    {
        Context->DeviceContext->Draw(vertexCount, 0);
    }
    return true;
}

bool FDecalRenderPass::End(const FRenderPassContext* Context)
{
    return true;
}
