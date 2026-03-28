#pragma once
#include <d3d11.h>
#include <CoreMinimal.h>

class CCore;
class CRenderer;

class CViewport
{
public:
	~CViewport();

	void Render(CCore* Core, CRenderer* Renderer, HWND Hwnd);
	void ReleaseSceneView();
	bool GetMousePositionInViewport(int32 WindowMouseX, int32 WindowMouseY, int32& OutViewportX, int32& OutViewportY, int32& OutWidth, int32& OutHeight) const;
	bool IsHovered() const { return bHovered; }
	bool IsFocused() const { return bFocused; }
	bool IsVisible() const { return bVisible; }

	void ReadySceneView(ID3D11Device* Device, uint32 Width, uint32 Height);

private:
	ID3D11Texture2D* RenderTargetTexture = nullptr;
	ID3D11RenderTargetView* RenderTargetView = nullptr;
	ID3D11ShaderResourceView* ShaderResourceView = nullptr;
	ID3D11Texture2D* DepthStencilTexture = nullptr;
	ID3D11DepthStencilView* DepthStencilView = nullptr;

	uint32 OffscreenWidth = 0;
	uint32 OffscreenHeight = 0;
	int32 ClientPosX = 0;
	int32 ClientPosY = 0;
	bool bHovered = false;
	bool bFocused = false;
	bool bVisible = false;
};

