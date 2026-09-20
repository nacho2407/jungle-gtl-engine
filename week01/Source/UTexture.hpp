#pragma once

#include <cassert>

#include <d3d11.h>

class UTexture
{
public:
	UTexture() {}

	UTexture(ID3D11Device* Device, ID3D11DeviceContext* DeviceContext, const char* FileName)
	{
		bool bIsOK = Initialize(Device, DeviceContext, FileName);
		assert(bIsOK);
	}

	~UTexture()
	{
		// Release와 동시에 객체 자체의 메모리도 해제되도록
		Release();
	}

	bool Initialize(ID3D11Device* Device, ID3D11DeviceContext* DeviceContext, const char* FileName);
	void Release();

	ID3D11ShaderResourceView* GetTextureSRV() { return TextureSRV; }

private:
	ID3D11Texture2D* Texture;
	ID3D11ShaderResourceView* TextureSRV;

	int Width;
	int Height;
	int Channels;
};
