#pragma once

#include <d3d11.h>

#include "CoreMinimal.h"

class FObjViewerViewportSurface
{
public:
	~FObjViewerViewportSurface();

	void SetSize(int32 InWidth, int32 InHeight);
	void EnsureResources(ID3D11Device* Device);
	void Release();

	bool IsValid() const;
	int32 GetWidth() const { return Width; }
	int32 GetHeight() const { return Height; }

	ID3D11RenderTargetView* GetRTV() const { return RenderTargetView; }
	ID3D11DepthStencilView* GetDSV() const { return DepthStencilView; }
	ID3D11ShaderResourceView* GetSRV() const { return ShaderResourceView; }

private:
	int32 Width = 0;
	int32 Height = 0;
	uint32 ResourceWidth = 0;
	uint32 ResourceHeight = 0;

	ID3D11Texture2D* RenderTargetTexture = nullptr;
	ID3D11RenderTargetView* RenderTargetView = nullptr;
	ID3D11ShaderResourceView* ShaderResourceView = nullptr;
	ID3D11Texture2D* DepthStencilTexture = nullptr;
	ID3D11DepthStencilView* DepthStencilView = nullptr;
};
