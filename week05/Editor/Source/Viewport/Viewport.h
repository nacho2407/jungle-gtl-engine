#pragma once

#include "ViewportTypes.h"
#include <d3d11.h>

class FViewport
{
public:
	~FViewport();

	void SetRect(const FRect& InRect);
	const FRect& GetRect() const { return Rect; }

	void EnsureResources(ID3D11Device* Device);
	void Release();

	ID3D11RenderTargetView* GetRTV() const { return RenderTargetView; }
	ID3D11DepthStencilView* GetDSV() const { return DepthStencilView; }
	ID3D11ShaderResourceView* GetSRV() const { return ShaderResourceView; }

private:
	FRect Rect;

	ID3D11Texture2D* RenderTargetTexture = nullptr;
	ID3D11RenderTargetView* RenderTargetView = nullptr;
	ID3D11ShaderResourceView* ShaderResourceView = nullptr;
	ID3D11Texture2D* DepthStencilTexture = nullptr;
	ID3D11DepthStencilView* DepthStencilView = nullptr;

	uint32 ResourceWidth = 0;
	uint32 ResourceHeight = 0;
};