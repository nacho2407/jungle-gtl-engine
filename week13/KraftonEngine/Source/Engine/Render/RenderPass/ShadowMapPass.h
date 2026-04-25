#pragma once

#include "Render/RenderPass/RenderPassBase.h"
#include "Render/Resource/Buffer.h"
#include "Math/Matrix.h"

/*
	FShadowMapPass — 라이트별 Shadow Depth 렌더링 패스.
	LightCulling(1)과 Opaque(2) 사이에 실행되며,
	각 라이트의 ViewProj로 depth-only 렌더링을 수행합니다.
	현재 Spot Light 1개를 지원하며, 향후 Directional/Point로 확장 예정.
*/
class FShadowMapPass final : public FRenderPassBase
{
public:
	FShadowMapPass();
	~FShadowMapPass();

	void Execute(const FPassContext& Ctx) override;
	void EndPass(const FPassContext& Ctx) override;

	// Shadow Map SRV — Opaque 셰이더에서 샘플링용
	ID3D11ShaderResourceView* GetShadowMapSRV() const { return ShadowSRV; }

	// Light ViewProj — Opaque 셰이더에서 shadow coordinate 변환용
	const FMatrix& GetLightViewProj() const { return LightViewProj; }

	// 유효한 shadow가 있는지
	bool HasValidShadow() const { return bHasValidShadow; }


private:
	void EnsureShadowMap(ID3D11Device* Device, uint32 Size);
	void ReleaseShadowMap();


private:
	// Shadow map GPU 리소스
	ID3D11Texture2D*          ShadowTexture = nullptr;
	ID3D11DepthStencilView*   ShadowDSV     = nullptr;
	ID3D11ShaderResourceView* ShadowSRV     = nullptr;
	uint32 ShadowMapSize = 0;

	// Shadow 렌더링용 PerObject CB (b1)
	FConstantBuffer ShadowPerObjectCB;

	// 이번 프레임의 Light ViewProj (Opaque에서 참조)
	FMatrix LightViewProj;
	bool bHasValidShadow = false;

};
