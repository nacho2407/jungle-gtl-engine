#include "SandervistanRenderPass.h"
#include "Core/ResourceManager.h"

bool FSandevistanRenderPass::Initialize()
{
    return true;
}

bool FSandevistanRenderPass::Release()
{
    return true;
}

bool FSandevistanRenderPass::Begin(const FRenderPassContext* Context)
{
    const FRenderTargetSet* RT = Context->RenderTargets;

    // ⚠️ 안전: ping-pong 필요 (임시 RT)
    ID3D11RenderTargetView* RTVs[1] = { RT->SceneSandervistanRTV };
    Context->DeviceContext->OMSetRenderTargets(1, RTVs, nullptr);

    OutRTV = RT->SceneSandervistanRTV;
    OutSRV = RT->SceneSandervistanSRV;

    // 이전 패스 결과 입력
    ID3D11ShaderResourceView* srvs[] = { PrevPassSRV };
    Context->DeviceContext->PSSetShaderResources(0, 1, srvs);

    // Constant Buffer
    FSandevistanConstants cb = {};
    // cb.Time = Context->RenderBus->GetTime();
    cb.Intensity = Context->RenderBus->GetSandevistanIntensity();
    cb.InvResolution[0] = (Context->RenderTargets->Width > 0.0f) ? (1.0f / Context->RenderTargets->Width) : 0.0f;
    cb.InvResolution[1] = (Context->RenderTargets->Height > 0.0f) ? (1.0f / Context->RenderTargets->Height) : 0.0f;
    cb.Center[0] = 0.5f;
    cb.Center[1] = 0.5f;
    cb.InvResolution[0] = 1.0f / RT->Width;
    cb.InvResolution[1] = 1.0f / RT->Height;

    Context->RenderResources->SandevistanCB.Update(
        Context->DeviceContext,
        &cb,
        sizeof(cb));

    ID3D11Buffer* buffer = Context->RenderResources->SandevistanCB.GetBuffer();
    Context->DeviceContext->PSSetConstantBuffers(11, 1, &buffer);

    // Shader 바인딩
    UShader* shader = FResourceManager::Get().GetShader("Shaders/Multipass/SandervistanPass.hlsl");
    shader->Bind(Context->DeviceContext);

    // Fullscreen triangle
    Context->DeviceContext->IASetInputLayout(nullptr);
    Context->DeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    Context->DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    return true;
}

bool FSandevistanRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    Context->DeviceContext->Draw(3, 0);
    return true;
}

bool FSandevistanRenderPass::End(const FRenderPassContext* Context)
{
    ID3D11ShaderResourceView* nullSRVs[] = { nullptr };
    Context->DeviceContext->PSSetShaderResources(0, 1, nullSRVs);
    return true;
}