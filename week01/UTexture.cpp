#include "UTexture.hpp"

#include "stb_image.h"

bool UTexture::Initialize(ID3D11Device* Device, ID3D11DeviceContext* DeviceContext, const char* FileName)
{
	D3D11_TEXTURE2D_DESC TextureDesc;
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	UINT RowPitch;
	HRESULT Result;
	unsigned char* Data;

	Data = stbi_load(FileName, &Width, &Height, &Channels, 4);

	assert(Data);

	TextureDesc.Height = Height;
	TextureDesc.Width = Width;
	TextureDesc.MipLevels = 1;
	TextureDesc.ArraySize = 1;
	TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.SampleDesc.Quality = 0;
	TextureDesc.Usage = D3D11_USAGE_DEFAULT;
	TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	TextureDesc.CPUAccessFlags = 0;
	TextureDesc.MiscFlags = 0;

	RowPitch = (Width * 4) * sizeof(unsigned char);
	D3D11_SUBRESOURCE_DATA InitData = { Data, RowPitch, 0 };

	Result = Device->CreateTexture2D(&TextureDesc, &InitData, &Texture);
	if (FAILED(Result))
	{
		return false;
	}

	SRVDesc.Format = TextureDesc.Format;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	SRVDesc.Texture2D.MipLevels = 1;

	Result = Device->CreateShaderResourceView(Texture, &SRVDesc, &TextureSRV);
	if (FAILED(Result))
	{
		return false;
	}

	stbi_image_free(Data);
	return true;
}

void UTexture::Release()
{
	if (TextureSRV)
	{
		TextureSRV->Release();
		TextureSRV = nullptr;
	}
	if (Texture)
	{
		Texture->Release();
		Texture = nullptr;
	}
}
