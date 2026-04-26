#include "VSMConversionRenderPass.h"
#include "Render/Resource/ShadowAtlasManager.h"
#include "Core/ResourceManager.h"

bool FVSMConversionRenderPass::Initialize()
{
    return true;
}

bool FVSMConversionRenderPass::Release()
{
    return true;
}

bool FVSMConversionRenderPass::Begin(const FRenderPassContext* Context)
{


    ID3D11DeviceContext* DeviceContext = Context->DeviceContext;
    // VSMConversionRenderPass::Begin() 에 추가
    D3D11_BLEND_DESC BlendDesc = {};
    BlendDesc.RenderTarget[0].BlendEnable = FALSE; // Blend 완전히 끔
    BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    ID3D11BlendState* BlendState = nullptr;
    Context->Device->CreateBlendState(&BlendDesc, &BlendState);
    DeviceContext->OMSetBlendState(BlendState, nullptr, 0xFFFFFFFF);
    BlendState->Release();

	// DepthStencil 비활성화 State 생성 및 적용
    D3D11_DEPTH_STENCIL_DESC DSDesc = {};
    DSDesc.DepthEnable = FALSE;                          // Depth Test 끔
    DSDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // Depth Write 끔
    DSDesc.StencilEnable = FALSE;

    ID3D11DepthStencilState* DSState = nullptr;
    Context->Device->CreateDepthStencilState(&DSDesc, &DSState);
    DeviceContext->OMSetDepthStencilState(DSState, 0);

    // FVSMConversionRenderPass Begin에서
    ID3D11SamplerState* PointSampler = FResourceManager::Get().GetOrCreateSamplerState(ESamplerType::EST_Point);
    Context->DeviceContext->PSSetSamplers(2, 1, &PointSampler);
    //ID3D11Texture2D* VSMShadowMap = FShadowAtlasManager::Get().VarianceRTVShadowMap.Get();

	
	ID3D11RenderTargetView* VSMRTV = FShadowAtlasManager::Get().VarianceShadowRTV.Get();
    Context->DeviceContext->ClearRenderTargetView(VSMRTV, FShadowAtlasManager::Get().ClearColor);
	Context->DeviceContext->OMSetRenderTargets(1, &VSMRTV, nullptr);

	// initialize할 때 이미 묶어 놓았을 것
	ID3D11ShaderResourceView* ShadowMap = FShadowAtlasManager::Get().ShadowSRV.Get();
    Context->DeviceContext->PSSetShaderResources(10, 1, &ShadowMap);

	UShader* ShadowShader = FResourceManager::Get().GetShader("Shaders/VSMShadow.hlsl");
    ShadowShader->Bind(Context->DeviceContext); 
    Context->DeviceContext->IASetInputLayout(nullptr);
    Context->DeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    Context->DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	ID3D11Texture2D* VSMTexture = FShadowAtlasManager::Get().VarianceRTVShadowMap.Get();
    D3D11_TEXTURE2D_DESC VSMDesc = {};
    VSMTexture->GetDesc(&VSMDesc);

        D3D11_VIEWPORT ShadowViewport = {};
        ShadowViewport.TopLeftX = 0.0f;
        ShadowViewport.TopLeftY = 0.0f;
        ShadowViewport.Width = static_cast<float>(VSMDesc.Width);
        ShadowViewport.Height = static_cast<float>(VSMDesc.Height);
        ShadowViewport.MinDepth = 0.0f;
        ShadowViewport.MaxDepth = 1.0f;

        DeviceContext->RSSetViewports(1, &ShadowViewport);


    return true;
}

bool FVSMConversionRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    Context->DeviceContext->Draw(3, 0);
    return true;
}

bool FVSMConversionRenderPass::End(const FRenderPassContext* Context)
{
    ID3D11ShaderResourceView* ShadowMap = nullptr;
    ID3D11SamplerState* PointSampler = nullptr;

    Context->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    Context->DeviceContext->PSSetShaderResources(10, 1, &ShadowMap);
    Context->DeviceContext->PSSetSamplers(2, 1, &PointSampler);
	//VarianceShadowRTV 해제

	D3D11_VIEWPORT OriginViewport = {};
    OriginViewport.TopLeftX = 0.0f;
    OriginViewport.TopLeftY = 0.0f;
    OriginViewport.Width = static_cast<float>(Context->RenderBus->GetViewportSize().X);
    OriginViewport.Height = static_cast<float>(Context->RenderBus->GetViewportSize().Y);
    OriginViewport.MinDepth = 0.0f;
    OriginViewport.MaxDepth = 1.0f;

    Context->DeviceContext->RSSetViewports(1, &OriginViewport);

    return true;
}
