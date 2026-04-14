#include "FXAARenderPass.h"
#include "Core/ResourceManager.h"
#include <algorithm>

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
    ID3D11RenderTargetView* RTVs[1] = {
        RenderTargets->SceneFXAARTV
    };
    ID3D11DepthStencilView* DSV = nullptr;

    Context->DeviceContext->OMSetRenderTargets(ARRAYSIZE(RTVs), RTVs, DSV);
    OutSRV = RenderTargets->SceneFXAASRV;
    OutRTV = RenderTargets->SceneFXAARTV;

    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D11ShaderResourceView* srvs[] = { PrevPassSRV }; // FXAA 패스에서는 최종 조명 결과만 필요
	Context->DeviceContext->PSSetShaderResources(0, 1, srvs);

    FFXAAConstants FXAAConstants = {};
    FXAAConstants.InvResolution[0] = (Context->RenderTargets->Width > 0.0f) ? (1.0f / Context->RenderTargets->Width) : 0.0f;
    FXAAConstants.InvResolution[1] = (Context->RenderTargets->Height > 0.0f) ? (1.0f / Context->RenderTargets->Height) : 0.0f;
    FXAAConstants.bEnabled = Context->RenderBus->GetFXAAEnabled();
    Context->RenderResources->FXAAConstantBuffer.Update(Context->DeviceContext, &FXAAConstants, sizeof(FFXAAConstants));
    ID3D11Buffer* cb10 = Context->RenderResources->FXAAConstantBuffer.GetBuffer();

    Context->DeviceContext->PSSetConstantBuffers(10, 1, &cb10);

    UShader* FXAAShader = FResourceManager::Get().GetShader("Shaders/Multipass/FXAAPass.hlsl");
    FXAAShader->Bind(Context->DeviceContext);

    /**
     * 풀스크린 쿼드에 그리는데, mainVS 에서	정점 데이터를 생성하기 때문에 IA 단계에서 별도의
     * 버퍼 바인딩이 필요 없다.
     */
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
    return true;
}
