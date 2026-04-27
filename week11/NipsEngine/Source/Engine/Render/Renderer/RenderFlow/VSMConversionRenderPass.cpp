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

    // VSM 기록에 영향을 주는 blend state 초기화.

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


    D3D11_VIEWPORT ShadowViewport = {};
    ShadowViewport.TopLeftX = 0.0f;
    ShadowViewport.TopLeftY = 0.0f;
    ShadowViewport.Width = static_cast<float>(FShadowAtlasManager::Get().GetAtlasWidth());
    ShadowViewport.Height = static_cast<float>(FShadowAtlasManager::Get().GetAtlasHeight());
    ShadowViewport.MinDepth = 0.0f;
    ShadowViewport.MaxDepth = 1.0f;
    DeviceContext->RSSetViewports(1, &ShadowViewport);


    // constantbuffer는 shadowrenderpass에서 알아서 해줌ㄴ
    return true;
}

bool FVSMConversionRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    DrawVSMConversion(Context);
    DispatchHorizontalBlur(Context);
    DispatchVerticalBlur(Context);

    return true;
}

bool FVSMConversionRenderPass::End(const FRenderPassContext* Context)
{
    ID3D11ShaderResourceView* ShadowMap = nullptr;
    ID3D11SamplerState* PointSampler = nullptr;

    Context->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    Context->DeviceContext->PSSetShaderResources(10, 1, &ShadowMap);
    Context->DeviceContext->PSSetSamplers(2, 1, &PointSampler);
    // VarianceShadowRTV 해제

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

bool FVSMConversionRenderPass::DrawVSMConversion(const FRenderPassContext* Context)
{
    // FVSMConversionRenderPass Begin에서
    ID3D11SamplerState* PointSampler = FResourceManager::Get().GetOrCreateSamplerState(ESamplerType::EST_Point);
    Context->DeviceContext->PSSetSamplers(2, 1, &PointSampler);
    // ID3D11Texture2D* VSMShadowMap = FShadowAtlasManager::Get().VarianceRTVShadowMap.Get();


    // ID3D11RenderTargetView* VSMRTV = FShadowAtlasManager::Get().VarianceShadowRTV.Get();
    ID3D11RenderTargetView* VSMRTV = FShadowAtlasManager::Get().GetVarianceRTV();
    Context->DeviceContext->ClearRenderTargetView(VSMRTV, FShadowAtlasManager::Get().ClearColor);
    Context->DeviceContext->OMSetRenderTargets(1, &VSMRTV, nullptr);

    // initialize할 때 이미 묶어 놓았을 것 & Getting Normal ShadowMapSRV
    ID3D11ShaderResourceView* ShadowMap = FShadowAtlasManager::Get().GetSRV();
    Context->DeviceContext->PSSetShaderResources(10, 1, &ShadowMap);

    UShader* ShadowShader = FResourceManager::Get().GetShader("Shaders/VSMShadow.hlsl");
    ShadowShader->Bind(Context->DeviceContext);
    Context->DeviceContext->IASetInputLayout(nullptr);
    Context->DeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    Context->DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    Context->DeviceContext->Draw(3, 0);
    // depth ,depth^2 기록한 RTV를 SRV로 쓰기 위한 Unbind
    Context->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
}

bool FVSMConversionRenderPass::DispatchHorizontalBlur(const FRenderPassContext* Context)
{
    ID3D11DeviceContext* DeviceContext = Context->DeviceContext;
    // VarianceShadowSRV t10에 binding
	ID3D11ShaderResourceView* VSMSRV = FShadowAtlasManager::Get().GetVarianceSRV();
    DeviceContext->CSSetShaderResources(0, 1, &VSMSRV);
	
	// Horizontal Blur UAV binding
    ID3D11UnorderedAccessView* HorizonUAV = FShadowAtlasManager::Get().GetBlurUAV();
    DeviceContext->CSSetUnorderedAccessViews(0, 1, &HorizonUAV, nullptr);

	FConstantBuffer* ShadowConstantBuffer = &Context->RenderResources->ShadowBuffer;
    ID3D11Buffer* UAVShadowConstantbuffer = ShadowConstantBuffer->GetBuffer();
    DeviceContext->CSSetConstantBuffers(0, 1, &UAVShadowConstantbuffer);
 
 
	FComputeShader* Horizontal_CS = FResourceManager::Get().GetComputeShader("VSMBlur_H");
 	if (!Horizontal_CS)
	{
        return false;
	}
	
	Horizontal_CS->Bind(DeviceContext);
	// SRV 는 read - data slot
	// UAV 는 RW - data slot 


	// TileSize = 1024 * 1024 
	// atlas = 8192 * 8192s
    uint32 AtalsWidth = FShadowAtlasManager::Get().GetAtlasWidth();
    uint32 AtalsHeight = FShadowAtlasManager::Get().GetAtlasHeight();

    uint32 DispatchX = (AtalsWidth + 7) / 8;
    uint32 DispatchY = (AtalsHeight + 7) / 8;

    Horizontal_CS->Dispatch(DeviceContext, DispatchX, DispatchY, 1);
    Horizontal_CS->Unbind(DeviceContext);
    DeviceContext->CSSetUnorderedAccessViews(0, 1, nullptr, nullptr);
    DeviceContext->CSSetShaderResources(0, 1, nullptr);

}

bool FVSMConversionRenderPass::DispatchVerticalBlur(const FRenderPassContext* Context)
{
    ID3D11DeviceContext* DeviceContext = Context->DeviceContext;

	ID3D11ShaderResourceView* VerticalSRV = FShadowAtlasManager::Get().GetBlurSRV();
    DeviceContext->CSSetShaderResources(0, 1, &VerticalSRV);

	ID3D11UnorderedAccessView* VerticalUAV = FShadowAtlasManager::Get().GetVarianceUAV();
    DeviceContext->CSSetUnorderedAccessViews(0, 1, &VerticalUAV, nullptr);
	
	FComputeShader* Vertical_CS = FResourceManager::Get().GetComputeShader("VSMBlur_V");
    if (!Vertical_CS)
    {
        return false;
    }


    // TileSize = 1024 * 1024
    // atlas = 8192 * 8192s
    uint32 AtalsWidth = FShadowAtlasManager::Get().GetAtlasWidth();
    uint32 AtalsHeight = FShadowAtlasManager::Get().GetAtlasHeight();

    uint32 DispatchX = (AtalsWidth + 7) / 8;
    uint32 DispatchY = (AtalsHeight + 7) / 8;

	Vertical_CS->Bind(DeviceContext);
    Vertical_CS->Dispatch(DeviceContext, DispatchX, DispatchY, 1);
	Vertical_CS->Unbind(DeviceContext);
    DeviceContext->CSSetUnorderedAccessViews(0, 1, nullptr, nullptr);
    DeviceContext->CSSetShaderResources(0, 1, nullptr);
}
