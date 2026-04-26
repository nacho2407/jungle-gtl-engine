#pragma once

#include "Render/RenderPass/RenderPassBase.h"
#include "Render/Resource/Buffer.h"
#include "Render/Types/RenderConstants.h"
#include "Math/Matrix.h"

#include "Render/Resource/RenderResources.h"
#include "Render/Shadow/ShadowAtlasQuadTree.h"

/*
	FShadowMapPass — 라이트별 Shadow Depth 렌더링 패스.
	LightCulling(1)과 Opaque(2) 사이에 실행되며,
	각 라이트의 ViewProj로 depth-only 렌더링을 수행합니다.

	GPU 리소스는 FSystemResources::ShadowResources에서 소유하며,
	이 패스는 생성/갱신/바인딩만 담당합니다.
*/
class FShadowMapPass final : public FRenderPassBase
{
public:
	FShadowMapPass();
	~FShadowMapPass();

	void BeginPass(const FPassContext& Ctx) override;
	void Execute(const FPassContext& Ctx) override;
	void EndPass(const FPassContext& Ctx) override;

	// 유효한 shadow가 있는지
	bool HasValidShadow() const { return bHasValidShadow; }

	const FMatrix& GetLightViewProj() const { return LightViewProj; }

private:
	// Shadow 렌더링용 PerObject CB (b1)
	FConstantBuffer ShadowPerObjectCB;

	// 이번 프레임의 Light ViewProj
	FMatrix LightViewProj;
	bool bHasValidShadow = false;

	FShadowAtlasQuadTree SpotLightAtlas;
};
