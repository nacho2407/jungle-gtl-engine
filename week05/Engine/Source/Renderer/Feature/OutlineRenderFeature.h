#pragma once

#include "CoreMinimal.h"
#include <d3d11.h>

class FRenderer;
struct FRenderMesh;

struct ENGINE_API FOutlineRenderItem
{
	// 아웃라인 마스크 생성에 참여할 메시다.
	FRenderMesh* Mesh = nullptr;
	// 아웃라인 마스크를 그릴 때 적용할 월드 변환이다.
	FMatrix WorldMatrix = FMatrix::Identity;
};

struct ENGINE_API FOutlineRenderRequest
{
	// 아웃라인을 적용할 씬 아이템 목록이다.
	TArray<FOutlineRenderItem> Items;
	// 요청 객체를 유지한 채 이번 프레임만 비활성화할 수 있게 한다.
	bool bEnabled = true;
};

class ENGINE_API FOutlineRenderFeature
{
public:
	~FOutlineRenderFeature();

	// 아웃라인 마스크 생성과 후처리 합성 패스를 모두 실행한다.
	bool Render(FRenderer& Renderer, const FOutlineRenderRequest& Request);
	// 아웃라인 기능이 사용하는 셰이더, 상태, 임시 텍스처를 해제한다.
	void Release();

private:
	// 필요한 시점에 셰이더, 상태, 상수 버퍼를 지연 생성한다.
	bool Initialize(FRenderer& Renderer);
	// 현재 타깃 크기에 맞는 아웃라인 마스크 텍스처를 보장한다.
	bool EnsureOutlineMaskResources(FRenderer& Renderer, uint32 Width, uint32 Height);
	// 임시 아웃라인 마스크 텍스처와 뷰를 해제한다.
	void ReleaseOutlineMaskResources();
	// 아웃라인 후처리 상수를 업데이트한다.
	void UpdateOutlinePostConstantBuffer(FRenderer& Renderer, const FVector4& OutlineColor, float OutlineThickness, float OutlineThreshold);

private:
	ID3D11Buffer* OutlinePostConstantBuffer = nullptr;
	ID3D11DepthStencilState* StencilWriteState = nullptr;
	ID3D11DepthStencilState* StencilEqualState = nullptr;
	ID3D11DepthStencilState* StencilNotEqualState = nullptr;
	ID3D11BlendState* OutlineBlendState = nullptr;
	ID3D11RasterizerState* OutlineRasterizerState = nullptr;
	ID3D11SamplerState* OutlineSampler = nullptr;
	ID3D11VertexShader* OutlinePostVS = nullptr;
	ID3D11PixelShader* OutlineMaskPS = nullptr;
	ID3D11PixelShader* OutlineSobelPS = nullptr;
	ID3D11Texture2D* OutlineMaskTexture = nullptr;
	ID3D11RenderTargetView* OutlineMaskRTV = nullptr;
	ID3D11ShaderResourceView* OutlineMaskSRV = nullptr;
	uint32 OutlineMaskWidth = 0;
	uint32 OutlineMaskHeight = 0;
};
