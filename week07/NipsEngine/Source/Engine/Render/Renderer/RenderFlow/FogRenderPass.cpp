#include "FogRenderPass.h"
#include "Core/ResourceManager.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Scene/RenderCommand.h"
#include <algorithm>

bool FFogRenderPass::Initialize()
{
    return true;
}

bool FFogRenderPass::Release()
{
    return true;
}

bool FFogRenderPass::Begin(const FRenderPassContext* Context)
{
    const TArray<FRenderCommand>& Commands = Context->RenderBus->GetCommands(ERenderPass::Fog);
    if (Commands.empty())
    {
        // Fog command가 없으면 이전 패스 결과를 그대로 넘긴다.
        OutSRV = PrevPassSRV;
        OutRTV = PrevPassRTV;
        return true;
    }

    ID3D11RenderTargetView* OutputRTV =
        Context->RenderTargets->ResolvePostProcessOutputRTV(PrevPassRTV);
    ID3D11ShaderResourceView* OutputSRV =
        Context->RenderTargets->ResolvePostProcessOutputSRV(PrevPassRTV);

    if (!OutputRTV ||
        !OutputSRV ||
        !PrevPassSRV ||
        !Context->RenderTargets->SceneDepthSRV ||
        !Context->RenderTargets->SceneWorldPosSRV)
    {
        return false;
    }

    UShader* FogPassShader = FResourceManager::Get().GetShader("Shaders/Multipass/FogPass.hlsl");
    if (FogPassShader)
    {
        FogPassShader->Bind(Context->DeviceContext);
    }

    ID3D11DepthStencilState* DSState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::StencilWrite);
    Context->DeviceContext->OMSetDepthStencilState(DSState, 1);

    ID3D11BlendState* BlendState = FResourceManager::Get().GetOrCreateBlendState(EBlendType::Opaque);
    Context->DeviceContext->OMSetBlendState(BlendState, nullptr, 0xFFFFFFFF);

    ID3D11RenderTargetView* RTVs[3] = { OutputRTV, nullptr, nullptr };
    Context->DeviceContext->OMSetRenderTargets(3, RTVs, nullptr);

    Context->DeviceContext->IASetInputLayout(nullptr);
    Context->DeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    Context->DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    OutSRV = OutputSRV;
    OutRTV = OutputRTV;
    return true;
}

bool FFogRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    const TArray<FRenderCommand>& Commands = Context->RenderBus->GetCommands(ERenderPass::Fog);
    if (Commands.empty())
    {
        return true;
    }

    ID3D11ShaderResourceView* srvs[] = {
        Context->RenderTargets->SceneDepthSRV,
        PrevPassSRV,
        Context->RenderTargets->SceneWorldPosSRV
    };
    Context->DeviceContext->PSSetShaderResources(0, 3, srvs);

    FFogPassConstants FogPassConstants = {};
    FogPassConstants.FogCount = std::min<uint32>(static_cast<uint32>(Commands.size()), MaxFogLayerCount);
    for (uint32 FogIndex = 0; FogIndex < FogPassConstants.FogCount; ++FogIndex)
    {
        FogPassConstants.Layers[FogIndex] = Commands[FogIndex].Constants.Fog;
    }

    Context->RenderResources->FogPassConstantBuffer.Update(Context->DeviceContext, &FogPassConstants, sizeof(FFogPassConstants));
    ID3D11Buffer* cb9 = Context->RenderResources->FogPassConstantBuffer.GetBuffer();
    Context->DeviceContext->VSSetConstantBuffers(9, 1, &cb9);
    Context->DeviceContext->PSSetConstantBuffers(9, 1, &cb9);

    Context->DeviceContext->Draw(3, 0);

    return true;
}

bool FFogRenderPass::End(const FRenderPassContext* Context)
{
    ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr, nullptr, nullptr, nullptr };
    Context->DeviceContext->PSSetShaderResources(0, 5, nullSRVs);
    return true;
}
