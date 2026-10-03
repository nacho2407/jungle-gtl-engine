#include "TranslucentRenderPass.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/Material.h"
#include "Render/Resource/ShaderHelper.h"
#include "Render/Resource/VertexFactoryTypes.h"
#include "Core/ResourceManager.h"

namespace
{
    uint32 BuildTranslucentPermutationKey(const FRenderPassContext* Context, const UMaterialInterface* Material)
    {
        uint32 PermutationKey = (uint32)ELightingModel::Unlit;
        switch (Context->RenderBus->GetViewMode())
        {
        case EViewMode::Lit_Gouraud:
            PermutationKey = (uint32)ELightingModel::Gouraud;
            break;
        case EViewMode::Lit_Lambert:
            PermutationKey = (uint32)ELightingModel::Lambert;
            break;
        case EViewMode::Lit_BlinnPhong:
            PermutationKey = (uint32)ELightingModel::BlinnPhong;
            break;
        case EViewMode::Heatmap:
            PermutationKey = (uint32)ELightingModel::Heatmap;
            break;
        }

        if (Material)
        {
            if (Material->HasDiffuseMap()) PermutationKey |= (uint32)EShaderFeature::HasDiffuseMap;
            if (Material->HasNormalMap()) PermutationKey |= (uint32)EShaderFeature::HasNormalMap;
            if (Material->HasSpecularMap()) PermutationKey |= (uint32)EShaderFeature::HasSpecularMap;
            if (Material->HasEmissiveMap()) PermutationKey |= (uint32)EShaderFeature::HasEmissiveMap;
            if (Material->HasAlphaMask()) PermutationKey |= (uint32)EShaderFeature::HasAlphaMask;
        }

        return PermutationKey;
    }

    FShaderProgram* GetTranslucentShaderProgram(const FRenderCommand& Cmd, uint32 PermutationKey)
    {
        if (!Cmd.Material)
        {
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

    bool DrawMeshCommandsForPass(const FRenderPassContext* Context, ERenderPass Pass)
    {
        const TArray<FRenderCommand>& Commands = Context->RenderBus->GetCommands(Pass);
        if (Commands.empty())
        {
            return true;
        }

        for (const FRenderCommand& Cmd : Commands)
        {
            Context->RenderResources->PerObjectConstantBuffer.Update(
                Context->DeviceContext,
                &Cmd.PerObjectConstants,
                sizeof(FPerObjectConstants));
            ID3D11Buffer* cb1 = Context->RenderResources->PerObjectConstantBuffer.GetBuffer();
            Context->DeviceContext->VSSetConstantBuffers(1, 1, &cb1);
            Context->DeviceContext->PSSetConstantBuffers(1, 1, &cb1);

            if (Cmd.MeshBuffer == nullptr || !Cmd.MeshBuffer->IsValid())
            {
                continue;
            }

            uint32 offset = 0;
            ID3D11Buffer* vertexBuffer = Cmd.MeshBuffer->GetVertexBuffer().GetBuffer();
            if (vertexBuffer == nullptr)
            {
                continue;
            }

            uint32 vertexCount = Cmd.MeshBuffer->GetVertexBuffer().GetVertexCount();
            uint32 stride = Cmd.MeshBuffer->GetVertexBuffer().GetStride();
            if (vertexCount == 0 || stride == 0)
            {
                continue;
            }

            if (Cmd.Material != nullptr)
            {
                const uint32 PermutationKey = BuildTranslucentPermutationKey(Context, Cmd.Material);
                FShaderProgram* Program = GetTranslucentShaderProgram(Cmd, PermutationKey);
                if (!Program)
                {
                    continue;
                }

                Program->Bind(Context->DeviceContext);
                Cmd.Material->BindRenderStates(Context->DeviceContext);
                Cmd.Material->BindParameters(Context->DeviceContext, Program->PS);
                BindVertexFactoryResources(Context->DeviceContext, Cmd.VertexFactoryType, Cmd);
            }

            Context->DeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

            ID3D11Buffer* indexBuffer = Cmd.MeshBuffer->GetIndexBuffer().GetBuffer();
            if (indexBuffer != nullptr)
            {
                Context->DeviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
                Context->DeviceContext->DrawIndexed(Cmd.SectionIndexCount, Cmd.SectionIndexStart, 0);
            }
            else
            {
                Context->DeviceContext->Draw(vertexCount, 0);
            }
        }

        return true;
    }
}

bool FTranslucentRenderPass::Initialize()
{
    return true;
}

bool FTranslucentRenderPass::Release()
{
    return true;
}

bool FTranslucentRenderPass::Begin(const FRenderPassContext* Context)
{
    ID3D11RenderTargetView* RTV = PrevPassRTV;
    ID3D11DepthStencilView* DSV = Context->RenderTargets->DepthStencilView;
    Context->DeviceContext->OMSetRenderTargets(1, &RTV, DSV);
    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    OutSRV = PrevPassSRV;
    OutRTV = PrevPassRTV;

    return true;
}

bool FTranslucentRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    return DrawMeshCommandsForPass(Context, ERenderPass::Translucent);
}

bool FTranslucentRenderPass::End(const FRenderPassContext* Context)
{
    return true;
}
