#include "PostProcessOutlineRenderPass.h"
#include "Core/ResourceManager.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Resource/Material.h"

namespace
{
    FShaderProgram* GetOutlineShaderProgram(const UMaterialInterface* Material)
    {
        if (!Material)
        {
            return nullptr;
        }

        FShaderStageKey VSKey;
        VSKey.FilePath = Material->GetPixelShaderPath();
        VSKey.EntryPoint = "VS";
        VSKey.Target = "vs_5_0";

        FShaderStageKey PSKey;
        PSKey.FilePath = Material->GetPixelShaderPath();
        PSKey.EntryPoint = Material->GetPixelShaderEntryPoint();
        PSKey.Target = "ps_5_0";

        return FResourceManager::Get().GetOrCreateShaderProgram(VSKey, PSKey);
    }
}

bool FPostProcessOutlineRenderPass::Initialize()
{
    return true;
}

bool FPostProcessOutlineRenderPass::Release()
{
    return true;
}

bool FPostProcessOutlineRenderPass::Begin(const FRenderPassContext* Context)
{
    ID3D11RenderTargetView* RTV = PrevPassRTV;
    Context->DeviceContext->OMSetRenderTargets(1, &RTV, nullptr);

    ID3D11ShaderResourceView* maskSRV = Context->RenderTargets->SelectionMaskSRV;
    Context->DeviceContext->PSSetShaderResources(7, 1, &maskSRV);

    Context->DeviceContext->IASetInputLayout(nullptr);
    Context->DeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    Context->DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    OutSRV = PrevPassSRV;
    OutRTV = PrevPassRTV;
    return true;
}

bool FPostProcessOutlineRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    const TArray<FRenderCommand>& Commands = Context->RenderBus->GetCommands(ERenderPass::PostProcessOutline);
    if (Commands.empty())
    {
        return true;
    }

    for (const FRenderCommand& Cmd : Commands)
    {
        if (Cmd.Material != nullptr)
        {
            FShaderProgram* Program = GetOutlineShaderProgram(Cmd.Material);
            if (!Program)
            {
                continue;
            }

            Program->Bind(Context->DeviceContext);
            Cmd.Material->BindRenderStates(Context->DeviceContext);
            Cmd.Material->BindParameters(Context->DeviceContext, Program->PS);
        }
        Context->DeviceContext->Draw(3, 0);
    }

    return true;
}

bool FPostProcessOutlineRenderPass::End(const FRenderPassContext* Context)
{
    ID3D11ShaderResourceView* nullSRV = nullptr;
    Context->DeviceContext->PSSetShaderResources(7, 1, &nullSRV);
    return true;
}
