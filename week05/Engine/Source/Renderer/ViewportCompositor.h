#pragma once

#include "CoreMinimal.h"
#include <d3d11.h>

struct ENGINE_API FViewportCompositeRect
{
	int32 X = 0;
	int32 Y = 0;
	int32 Width = 0;
	int32 Height = 0;

	// 실제로 그릴 수 있는 크기면 true를 반환한다.
	bool IsValid() const { return Width > 0 && Height > 0; }
};

struct ENGINE_API FViewportCompositeItem
{
	// 뷰포트 씬 패스가 출력한 장면 컬러 텍스처다.
	ID3D11ShaderResourceView* SceneColorSRV = nullptr;
	// 장면 텍스처를 백버퍼 어디에 놓을지 나타내는 사각형이다.
	FViewportCompositeRect Rect = {};
	// 리스트를 다시 만들지 않고 합성만 건너뛸 수 있게 한다.
	bool bVisible = true;
};

class ENGINE_API FViewportCompositor
{
public:
	FViewportCompositor() = default;
	~FViewportCompositor();

	FViewportCompositor(const FViewportCompositor&) = delete;
	FViewportCompositor& operator=(const FViewportCompositor&) = delete;

	// 뷰포트 합성에 필요한 셰이더와 고정 기능 상태를 생성한다.
	bool Initialize(ID3D11Device* Device);
	// 뷰포트 합성에 쓰는 자원을 해제한다.
	void Release();
	// 전달받은 뷰포트 장면 텍스처를 현재 백버퍼에 배치해 그린다.
	bool Compose(ID3D11DeviceContext* Context, const TArray<FViewportCompositeItem>& Items) const;

private:
	ID3D11VertexShader* BlitVertexShader = nullptr;
	ID3D11PixelShader* BlitPixelShader = nullptr;
	ID3D11SamplerState* PointSampler = nullptr;
	ID3D11DepthStencilState* NoDepthState = nullptr;
	ID3D11RasterizerState* ScissorRasterizerState = nullptr;
	bool bInitialized = false;
};
