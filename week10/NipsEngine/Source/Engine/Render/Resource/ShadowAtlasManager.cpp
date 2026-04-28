#include "ShadowAtlasManager.h"

namespace
{
	int32 QuantizeShadowSize(const int32 RequestTileSize)
	{
		if (RequestTileSize <= 0)
		{
			return AtlasSizeTier[0];
		}

		for (int32 i = 0; i < TierCount; ++i)
		{
			if (RequestTileSize <= AtlasSizeTier[i])
			{
				return AtlasSizeTier[i];
			}
		}

		return AtlasSizeTier[TierCount - 1];
	}
}

void FShadowAtlasManager::Initialize(ID3D11Device* InDevice)
{
	if (InDevice == nullptr) return;
    RootNode = new Node(0, 0, ShadowAtlasResolution2D, ShadowAtlasResolution2D);

	ShadowMapAtlas.Initialize(InDevice);
    ShadowCubeMapArray.Initialize(InDevice);
}

bool FShadowAtlasManager::AllocateTile(int32 ResolutionScale, FShadowAtlasTile& OutTile)
{
    int32 RequestTileSize = QuantizeShadowSize(ResolutionScale);

	int32 X, Y;
    if (RootNode->Insert(RequestTileSize, X, Y))
    {
        OutTile.X = X;
        OutTile.Y = Y;

        OutTile.Width = RequestTileSize;
        OutTile.Height = RequestTileSize;

        OutTile.ScaleOffset = FVector4(
            static_cast<float> (RequestTileSize) / ShadowAtlasResolution2D,
            static_cast<float> (RequestTileSize) / ShadowAtlasResolution2D,
            static_cast<float>(OutTile.X) / ShadowAtlasResolution2D,
            static_cast<float>(OutTile.Y) / ShadowAtlasResolution2D);

        return true;
    }
	return false;
}

//void FShadowAtlasManager::VSMInitialize(ID3D11Device* InDevice)
//{
//    if (InDevice == nullptr)
//        return;
//
//    Device = InDevice;
//
//    D3D11_TEXTURE2D_DESC texDesc = {};
//    texDesc.Width = 1024;
//    texDesc.Height = 1024;
//    texDesc.MipLevels = 1;
//    texDesc.ArraySize = 1;
//    texDesc.SampleDesc.Count = 1;
//    texDesc.Usage = D3D11_USAGE_DEFAULT;
//
//    // RTV용 텍스처
//    texDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
//    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
//    Device.Get()->CreateTexture2D(&texDesc, nullptr, &VarianceRTVShadowMap);
//
//    //// DSV용 텍스처 - Format과 BindFlags만 바꿔서 재사용
//    // texDesc.Format = DXGI_FORMAT_D32_FLOAT;
//    // texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
//    // Device->CreateTexture2D(&texDesc, nullptr, &VarianceDepthShadowMap);
//
//
//    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
//    rtvDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
//    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
//    rtvDesc.Texture2D.MipSlice = 0;
//    Device.Get()->CreateRenderTargetView(VarianceRTVShadowMap.Get(), &rtvDesc, &VarianceShadowRTV);
//
//    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
//    srvDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
//    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
//    srvDesc.Texture2D.MipLevels = 1;
//    srvDesc.Texture2D.MostDetailedMip = 0;
//    Device.Get()->CreateShaderResourceView(VarianceRTVShadowMap.Get(), &srvDesc, &VarianceShadowSRV);
//
//
//    // D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
//    //   dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
//    //   dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
//    //   dsvDesc.Texture2D.MipSlice = 0;
//    //   Device.Get()->CreateDepthStencilView(VarianceDepthShadowMap.Get(), &dsvDesc, &VarianceShadowDSV);
//}

TArray<FShadowAtlasTile> FShadowAtlasManager::GetAllocatedTiles() const
{
    TArray<FShadowAtlasTile> Result;
    GetLeafRects(Result);
    return Result;
}
    // D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    //   dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    //   dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    //   dsvDesc.Texture2D.MipSlice = 0;
    //   Device.Get()->CreateDepthStencilView(VarianceDepthShadowMap.Get(), &dsvDesc, &VarianceShadowDSV);

bool FShadowAtlasManager::AllocateTileCube(int32& OutCubeIndex)
{
    return ShadowCubeMapArray.AllocateCube(OutCubeIndex);
}

void FShadowAtlasManager::UpdateCubeDebugFace(ID3D11DeviceContext* DeviceContext, int32 CubeIndex, int32 FaceIndex)
{
    if (DeviceContext == nullptr ||
        CubeIndex < 0 || CubeIndex >= MAX_SHADOW_CUBES ||
        FaceIndex < 0 || FaceIndex >= CUBE_FACE_COUNT)
    {
        return;
    }

    ID3D11Texture2D* SourceTexture = ShadowCubeMapArray.CubeShadowMap.Get();
    ID3D11Texture2D* DebugTexture = ShadowCubeMapArray.CubeDebugTexture[CubeIndex][FaceIndex].Get();
    if (SourceTexture == nullptr || DebugTexture == nullptr)
    {
        return;
    }

    const uint32 SourceSlice = static_cast<uint32>(FaceIndex + CubeIndex * CUBE_FACE_COUNT);
    const uint32 SourceSubresource = D3D11CalcSubresource(0, SourceSlice, 1);
    DeviceContext->CopySubresourceRegion(
        DebugTexture,
        0,
        0,
        0,
        0,
        SourceTexture,
        SourceSubresource,
        nullptr);
}

ID3D11DepthStencilView* FShadowAtlasManager::GetCubeDSV(int32 CubeIndex, int32 FaceIndex) const
{
    if (CubeIndex >= MAX_SHADOW_CUBES || FaceIndex >= 6)
    {
        return nullptr;
    }
    return ShadowCubeMapArray.CubeDSV[CubeIndex][FaceIndex].Get();
}

ID3D11ShaderResourceView* FShadowAtlasManager::GetCubeDebugSRV(int32 CubeIndex, int32 FaceIndex) const
{
	if (CubeIndex >= MAX_SHADOW_CUBES || FaceIndex >= 6)
	{
		return nullptr;
    }
    return ShadowCubeMapArray.CubeDebugSRV[CubeIndex][FaceIndex].Get();
}
