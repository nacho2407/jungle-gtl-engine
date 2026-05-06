#include "PostProcessRenderPass.h"
#include "Core/ResourceManager.h"

bool FPostProcessRenderPass::Initialize()
{
    return true;
}

bool FPostProcessRenderPass::Release()
{
    return true;
}

bool FPostProcessRenderPass::Begin(const FRenderPassContext* Context)
{
    ID3D11RenderTargetView* RTV = PrevPassRTV;

    ID3D11RenderTargetView* RTVs[1] = { RTV };
    Context->DeviceContext->OMSetRenderTargets(1, RTVs, nullptr);

    OutSRV = PrevPassSRV;
    OutRTV = PrevPassRTV;

    // 이전 패스 결과 입력
    ID3D11ShaderResourceView* srvs[] = { PrevPassSRV };
    Context->DeviceContext->PSSetShaderResources(0, 1, srvs);

    // Shader 바인딩
    UShader* shader = FResourceManager::Get().GetShader("Shaders/Multipass/PostProcess.hlsl");
    shader->Bind(Context->DeviceContext);

    // Fullscreen triangle
    Context->DeviceContext->IASetInputLayout(nullptr);
    Context->DeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    Context->DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    return true;
}

bool FPostProcessRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    Context->DeviceContext->Draw(3, 0);
    return true;
}

bool FPostProcessRenderPass::End(const FRenderPassContext* Context)
{
    ID3D11ShaderResourceView* nullSRVs[] = { nullptr };
    Context->DeviceContext->PSSetShaderResources(0, 1, nullSRVs);
    return true;
}
