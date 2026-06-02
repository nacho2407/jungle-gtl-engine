#pragma once

#include "Render/RenderPass/RenderPassBase.h"
#include "Render/Resource/Buffer.h"

class FDepthOfFieldPass final : public FRenderPassBase
{
public:
	FDepthOfFieldPass();
	~FDepthOfFieldPass() override;

	bool BeginPass(const FPassContext& Ctx) override;
	void Execute(const FPassContext& Ctx) override;
	void EndPass(const FPassContext& Ctx) override;

private:
	struct FRenderTarget
	{
		ID3D11Texture2D* Texture = nullptr;
		ID3D11RenderTargetView* RTV = nullptr;
		ID3D11ShaderResourceView* SRV = nullptr;

		void Release();
		bool Create(ID3D11Device* Device, uint32 Width, uint32 Height, const char* DebugName);
	};

	void EnsureResources(ID3D11Device* Device, uint32 Width, uint32 Height);
	void ReleaseResources();

private:
	FRenderTarget BackgroundSetupTarget;
	FRenderTarget ForegroundSetupTarget;
	FRenderTarget BackgroundTarget;
	FRenderTarget ForegroundTarget;
	FRenderTarget ForegroundHoleFillTarget;

	FConstantBuffer DofCB;
	uint32 TargetWidth = 0;
	uint32 TargetHeight = 0;
};
