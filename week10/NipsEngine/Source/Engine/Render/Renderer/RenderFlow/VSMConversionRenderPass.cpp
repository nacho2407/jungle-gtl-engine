#include "VSMConversionRenderPass.h"
#include "Render/Resource/ShadowAtlasManager.h"
#include "Render/Resource/ShaderHelper.h"
#include "Core/ResourceManager.h"

namespace
{
    bool ShouldRunVSMConversion(const FRenderPassContext* Context)
    {
        return Context && Context->RenderBus && Context->RenderBus->GetShadowFilterMode() == EShadowFilter::VSM;
    }
}

bool FVSMConversionRenderPass::Initialize()
{
    return true;
}

bool FVSMConversionRenderPass::Release()
{
    return true;
}

bool FVSMConversionRenderPass::HasUsedShadowTiles() const
{
    const TArray<FShadowAtlasTile> ShadowAtlasTiles = FShadowAtlasManager::Get().GetAllocatedTiles();
    for (const auto& Tile : ShadowAtlasTiles)
    {
        if (Tile.bUsed)
        {
            return true;
        }
    }

    return false;
}

bool FVSMConversionRenderPass::Begin(const FRenderPassContext* Context)
{
    bSkipThisFrame = true;
    OutSRV = PrevPassSRV;
    OutRTV = PrevPassRTV;
    if (PrevRasterizerState != nullptr)
    {
        PrevRasterizerState->Release();
        PrevRasterizerState = nullptr;
    }

    if (Context == nullptr || Context->RenderBus == nullptr || !Context->RenderBus->GetShowFlags().bShadow)
    {
        return true;
    }

    // Default shadows use PCF and sample the depth atlas directly.
    // VSM conversion should only run when the explicit VSM filter is selected.
    if (!ShouldRunVSMConversion(Context))
    {
        return true;
    }

    if (!HasUsedShadowTiles())
    {
        return true;
    }

    bSkipThisFrame = false;

    ID3D11DeviceContext* DeviceContext = Context->DeviceContext;
    DeviceContext->RSGetState(&PrevRasterizerState);

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

	// TODO : DepthStencil 따로 관리
    ID3D11DepthStencilState* DSState = nullptr;
    Context->Device->CreateDepthStencilState(&DSDesc, &DSState);
    DeviceContext->OMSetDepthStencilState(DSState, 0);
    DSState->Release();


    D3D11_VIEWPORT ShadowViewport = {};
    ShadowViewport.TopLeftX = 0.0f;
    ShadowViewport.TopLeftY = 0.0f;
    ShadowViewport.Width = static_cast<float>(FShadowAtlasManager::Get().GetAtlasResolution());
    ShadowViewport.Height = static_cast<float>(FShadowAtlasManager::Get().GetAtlasResolution());
    ShadowViewport.MinDepth = 0.0f;
    ShadowViewport.MaxDepth = 1.0f;
    DeviceContext->RSSetViewports(1, &ShadowViewport);

    D3D11_RASTERIZER_DESC RasterizerDesc = {};
    RasterizerDesc.FillMode = D3D11_FILL_SOLID;
    RasterizerDesc.CullMode = D3D11_CULL_NONE;
    RasterizerDesc.DepthClipEnable = TRUE;
    RasterizerDesc.ScissorEnable = TRUE;

    ID3D11RasterizerState* ScissorRasterizerState = nullptr;
    HRESULT Hr = Context->Device->CreateRasterizerState(&RasterizerDesc, &ScissorRasterizerState);
    if (FAILED(Hr) || ScissorRasterizerState == nullptr)
    {
        DeviceContext->RSSetState(PrevRasterizerState);
        if (PrevRasterizerState != nullptr)
        {
            PrevRasterizerState->Release();
            PrevRasterizerState = nullptr;
        }
        bSkipThisFrame = true;
        return true;
    }

    DeviceContext->RSSetState(ScissorRasterizerState);
    if (ScissorRasterizerState != nullptr)
    {
        ScissorRasterizerState->Release();
    }

    // constantbuffer는 shadowrenderpass에서 알아서 해줌ㄴ
    return true;
}

bool FVSMConversionRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    if (bSkipThisFrame)
    {
        return true;
    }

    DrawVSMConversion(Context);
	// AtlasManager.VSMRTV를 통해서 depth , depth^2 기록
    DispatchHorizontalBlur(Context);
    DispatchVerticalBlur(Context);

    return true;
}

bool FVSMConversionRenderPass::End(const FRenderPassContext* Context)
{
    if (bSkipThisFrame)
    {
        return true;
    }

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
    Context->DeviceContext->RSSetState(PrevRasterizerState);
    if (PrevRasterizerState != nullptr)
    {
        PrevRasterizerState->Release();
        PrevRasterizerState = nullptr;
    }

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

    const TArray<FShadowAtlasTile> ShadowAtlasTiles = FShadowAtlasManager::Get().GetAllocatedTiles();
    for (const auto& Tile : ShadowAtlasTiles)
    {
        if (!Tile.bUsed)
        {
            continue;
        }

        const D3D11_RECT TileRect =
        {
            Tile.X,
            Tile.Y,
            Tile.X + Tile.Width,
            Tile.Y + Tile.Height
        };
        Context->DeviceContext->RSSetScissorRects(1, &TileRect);
        Context->DeviceContext->Draw(3, 0);
    }
    // depth ,depth^2 기록한 RTV를 SRV로 쓰기 위한 Unbind
    Context->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

	    // 추가: PS t10 명시적 해제 — VarianceShadowTexture UAV 바인딩 전에 반드시 정리
    ID3D11ShaderResourceView* NullSRV = nullptr;
    Context->DeviceContext->PSSetShaderResources(10, 1, &NullSRV);
    return true;
}

bool FVSMConversionRenderPass::DispatchHorizontalBlur(const FRenderPassContext* Context)
{

    ID3D11DeviceContext* DeviceContext = Context->DeviceContext;
    // VarianceShadowSRV t10에 binding
	ID3D11ShaderResourceView* VSMSRV = FShadowAtlasManager::Get().GetVarianceSRV();
	
	// Horizontal Blur UAV binding
    ID3D11UnorderedAccessView* HorizonUAV = FShadowAtlasManager::Get().GetBlurUAV();
    DeviceContext->CSSetUnorderedAccessViews(0, 1, &HorizonUAV, nullptr);
    DeviceContext->CSSetShaderResources(0, 1, &VSMSRV);



	FComputeShader* Horizontal_CS = FResourceManager::Get().GetComputeShader("VSMBlur_H");
 	if (!Horizontal_CS)
	{
        return false;
	}

	Horizontal_CS->Bind(DeviceContext);
	// SRV 는 read - data slot
	// UAV 는 RW - data slot 

	const TArray<FShadowAtlasTile> ShadowAtlasTile = FShadowAtlasManager::Get().GetAllocatedTiles();

	//constantbuffer는 B11에 바인딩하고 바로 해제할 것.
    for (const auto& Tile : ShadowAtlasTile)
    {	
		if (!Tile.bUsed)
            continue;
        FVSMBlurConstants CBData;
        CBData.AtlasOffsetX = static_cast<uint32>(Tile.X);            // 타일 좌상단 픽셀 X
        CBData.AtlasOffsetY = static_cast<uint32>(Tile.Y);            // 타일 좌상단 픽셀 Y
        CBData.TileWidth = static_cast<uint32>(Tile.Width);                // 타일 픽셀 너비
        CBData.TileHeight = static_cast<uint32>(Tile.Height);              // 타일 픽셀 높이

		FConstantBuffer* VSMTileConstantbuffer = &Context->RenderResources->VSMConstantBuffer;

		VSMTileConstantbuffer->Update(DeviceContext, &CBData, sizeof(FVSMBlurConstants));
        ID3D11Buffer* BlurCB = VSMTileConstantbuffer->GetBuffer();
		DeviceContext->CSSetConstantBuffers(11, 1, &BlurCB);

		// TileSize = 1024 * 1024
        // atlas = 8192 * 8192s
        uint32 AtalsWidth = FShadowAtlasManager::Get().GetAtlasResolution();
        uint32 AtalsHeight = FShadowAtlasManager::Get().GetAtlasResolution();

        uint32 DispatchX = (Tile.Width + 7) / 8;
        uint32 DispatchY = (Tile.Height + 7) / 8;

        Horizontal_CS->Dispatch(DeviceContext, DispatchX, DispatchY, 1);
    }

    Horizontal_CS->Unbind(DeviceContext);
    ID3D11UnorderedAccessView* NullUAV = nullptr;
    DeviceContext->CSSetUnorderedAccessViews(0, 1, &NullUAV, nullptr);

    ID3D11ShaderResourceView* NullSRV = nullptr;
    DeviceContext->CSSetShaderResources(0, 1, &NullSRV);

	ID3D11Buffer* NullCB = nullptr;
    DeviceContext->CSSetConstantBuffers(11, 1, &NullCB);
	return true;
}

bool FVSMConversionRenderPass::DispatchVerticalBlur(const FRenderPassContext* Context)
{
    ID3D11DeviceContext* DeviceContext = Context->DeviceContext;

	ID3D11ShaderResourceView* VerticalSRV = FShadowAtlasManager::Get().GetBlurSRV();

	ID3D11UnorderedAccessView* VerticalUAV = FShadowAtlasManager::Get().GetVarianceUAV();

	DeviceContext->CSSetShaderResources(0, 1, &VerticalSRV);
	DeviceContext->CSSetUnorderedAccessViews(0, 1, &VerticalUAV, nullptr);


	FComputeShader* Vertical_CS = FResourceManager::Get().GetComputeShader("VSMBlur_V");
    if (!Vertical_CS)
    {
        return false;
    }

	Vertical_CS->Bind(DeviceContext);

	const TArray<FShadowAtlasTile> ShadowAtlasTile = FShadowAtlasManager::Get().GetAllocatedTiles();

	// constantbuffer는 B11에 바인딩하고 바로 해제할 것.
    for (const auto& Tile : ShadowAtlasTile)
    {
        if (!Tile.bUsed)
            continue;

        FVSMBlurConstants CBData;
        CBData.AtlasOffsetX = static_cast<uint32>(Tile.X);            // 타일 좌상단 픽셀 X
        CBData.AtlasOffsetY = static_cast<uint32>(Tile.Y);            // 타일 좌상단 픽셀 Y
        CBData.TileWidth = static_cast<uint32>(Tile.Width);                // 타일 픽셀 너비
        CBData.TileHeight = static_cast<uint32>(Tile.Height);              // 타일 픽셀 높이

        FConstantBuffer* VSMTileConstantbuffer = &Context->RenderResources->VSMConstantBuffer;

        VSMTileConstantbuffer->Update(DeviceContext, &CBData, sizeof(FVSMBlurConstants));
        ID3D11Buffer* BlurCB = VSMTileConstantbuffer->GetBuffer();

        DeviceContext->CSSetConstantBuffers(11, 1, &BlurCB);

        // TileSize = 1024 * 1024
        // atlas = 8192 * 8192s
        //uint32 AtalsWidth = FShadowAtlasManager::Get().GetAtlasResolution();
        //uint32 AtalsHeight = FShadowAtlasManager::Get().GetAtlasResolution();

		uint32 DispatchX = (Tile.Width + 7) / 8;
        uint32 DispatchY = (Tile.Height + 7) / 8;


        Vertical_CS->Dispatch(DeviceContext, DispatchX, DispatchY, 1);
    }


	Vertical_CS->Unbind(DeviceContext);
    ID3D11UnorderedAccessView* NullUAV = nullptr;
    DeviceContext->CSSetUnorderedAccessViews(0, 1, &NullUAV, nullptr);

    ID3D11ShaderResourceView* NullSRV = nullptr;
    DeviceContext->CSSetShaderResources(0, 1, &NullSRV);

	ID3D11Buffer* NullCB = nullptr;
    DeviceContext->CSSetConstantBuffers(11, 1, &NullCB);
    return true;
}
