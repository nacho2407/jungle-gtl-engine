#pragma once

#include "Render/RenderPass/RenderPassBase.h"
#include "Render/Resource/Buffer.h"

class FBloomPass final : public FRenderPassBase
{
public:
	FBloomPass();
	bool BeginPass(const FPassContext& Ctx) override;
	void EndPass(const FPassContext& Ctx) override;

	void Execute(const FPassContext& Ctx) override;

private:
	FConstantBuffer BloomBlurCB;
};
