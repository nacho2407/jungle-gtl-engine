#pragma once
#include "RenderPass.h"

class FConstantBuffer;
class UShader;
struct FShadowConstants;
struct ID3D11DepthStencilView;
struct D3D11_VIEWPORT;

struct FCascadeSplit
{
	float NearDistance = { 0.f };
	float FarDistance = { 0.f };

	float SplitNearRatio = { 0.f };
	float SplitFarRatio = { 0.f };
};


class FShadowPass : public FBaseRenderPass
{
public:
	bool Initialize() override;
	bool Release() override;

protected:
	bool Begin(const FRenderPassContext* Context) override;
	bool DrawCommand(const FRenderPassContext* Context) override;
	bool End(const FRenderPassContext* Context) override;

private:
	void BuildPracticalCascadeSplit(float CamNear, float CamFar, float MaxShadowDistance, float Lambda, FCascadeSplit OutSplit[4]);
	void RenderShadowDepth(
		const FRenderPassContext* Context,
		FConstantBuffer* ShadowBuffer,
		UShader* ShadowShader,
		const TArray<FRenderCommand>& OpaqueCmds,
		ID3D11DepthStencilView* ShadowDSV,
		const D3D11_VIEWPORT& ShadowViewport,
		uint32 ShadowKey,
		const FShadowConstants& ShadowData);
};
