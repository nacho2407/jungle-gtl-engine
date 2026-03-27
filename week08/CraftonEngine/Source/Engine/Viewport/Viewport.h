#pragma once

#include "Render/Common/RenderTypes.h"

class FViewportClient;

// UE의 FViewport 대응 — 오프스크린 렌더 타깃 + D3D 리소스
class FViewport
{
public:
	FViewport() = default;
	~FViewport();

	// D3D 리소스 생성/해제/리사이즈
	bool Initialize(ID3D11Device* InDevice, uint32 InWidth, uint32 InHeight);
	void Release();
	void Resize(uint32 InWidth, uint32 InHeight);

	// 지연 리사이즈 — ImGui 렌더 중에 요청, RenderViewport 직전에 적용
	void RequestResize(uint32 InWidth, uint32 InHeight);
	bool ApplyPendingResize();

	// ViewportClient 참조
	void SetClient(FViewportClient* InClient) { ViewportClient = InClient; }
	FViewportClient* GetClient() const { return ViewportClient; }

	// 크기
	uint32 GetWidth() const { return Width; }
	uint32 GetHeight() const { return Height; }

	// D3D 리소스 접근자
	ID3D11RenderTargetView* GetRTV() const { return RTV; }
	ID3D11ShaderResourceView* GetSRV() const { return SRV; }
	ID3D11DepthStencilView* GetDSV() const { return DSV; }
	const D3D11_VIEWPORT& GetViewportRect() const { return ViewportRect; }

private:
	bool CreateResources();
	void ReleaseResources();

private:
	FViewportClient* ViewportClient = nullptr;

	ID3D11Device* Device = nullptr;

	// 렌더 타깃
	ID3D11Texture2D* RTTexture = nullptr;
	ID3D11RenderTargetView* RTV = nullptr;
	ID3D11ShaderResourceView* SRV = nullptr;		// ImGui::Image() 출력용

	// 뎁스/스텐실
	ID3D11Texture2D* DepthTexture = nullptr;
	ID3D11DepthStencilView* DSV = nullptr;

	D3D11_VIEWPORT ViewportRect = {};

	uint32 Width = 0;
	uint32 Height = 0;

	// 지연 리사이즈 요청
	uint32 PendingWidth = 0;
	uint32 PendingHeight = 0;
	bool bPendingResize = false;
};
