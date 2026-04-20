#include "FXAARenderPass.h"
#include "Core/ResourceManager.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Resource/RenderResources.h"

bool FFXAARenderPass::Initialize()
{
    return true;
}

bool FFXAARenderPass::Release()
{
    return true;
}

bool FFXAARenderPass::Begin(const FRenderPassContext* Context)
{
    const FRenderTargetSet* RenderTargets = Context->RenderTargets;
    if (PrevPassSRV == nullptr || PrevPassRTV == nullptr)
    {
        OutSRV = PrevPassSRV;
        OutRTV = PrevPassRTV;
        return true;
    }

    ID3D11RenderTargetView* OutputRTV = RenderTargets->ResolvePostProcessOutputRTV(PrevPassRTV);
    ID3D11ShaderResourceView* OutputSRV = RenderTargets->ResolvePostProcessOutputSRV(PrevPassRTV);
    if (OutputRTV == nullptr || OutputSRV == nullptr)
    {
        return false;
    }

    ID3D11RenderTargetView* RTVs[1] = { OutputRTV };
    Context->DeviceContext->OMSetRenderTargets(ARRAYSIZE(RTVs), RTVs, nullptr);
    OutSRV = OutputSRV;
    OutRTV = OutputRTV;

    ID3D11ShaderResourceView* srvs[] = { PrevPassSRV };
    Context->DeviceContext->PSSetShaderResources(0, 1, srvs);

    ID3D11SamplerState* LinearClampSampler =
        FResourceManager::Get().GetOrCreateSamplerState(ESamplerType::EST_LinearClamp, Context->Device);
    Context->DeviceContext->PSSetSamplers(0, 1, &LinearClampSampler);

    FFXAAConstants FXAAConstants = {};
    FXAAConstants.InvResolution[0] = (RenderTargets->Width > 0.0f) ? (1.0f / RenderTargets->Width) : 0.0f;
    FXAAConstants.InvResolution[1] = (RenderTargets->Height > 0.0f) ? (1.0f / RenderTargets->Height) : 0.0f;
    FXAAConstants.bEnabled = Context->RenderBus->GetFXAAEnabled();
    Context->RenderResources->FXAAConstantBuffer.Update(Context->DeviceContext, &FXAAConstants, sizeof(FFXAAConstants));
    ID3D11Buffer* cb10 = Context->RenderResources->FXAAConstantBuffer.GetBuffer();
    Context->DeviceContext->PSSetConstantBuffers(10, 1, &cb10);

    UShader* FXAAShader = FResourceManager::Get().GetShader("Shaders/Multipass/FXAAPass.hlsl");
    FXAAShader->Bind(Context->DeviceContext);

    Context->DeviceContext->IASetInputLayout(nullptr);
    Context->DeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    Context->DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    return true;
}

bool FFXAARenderPass::DrawCommand(const FRenderPassContext* Context)
{
    Context->DeviceContext->Draw(3, 0);
    return true;
}

bool FFXAARenderPass::End(const FRenderPassContext* Context)
{
    ID3D11ShaderResourceView* nullSRVs[] = { nullptr };
    Context->DeviceContext->PSSetShaderResources(0, 1, nullSRVs);

    ID3D11SamplerState* nullSampler = nullptr;
    Context->DeviceContext->PSSetSamplers(0, 1, &nullSampler);
    return true;
}
