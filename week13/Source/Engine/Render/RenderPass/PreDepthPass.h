#pragma once

#include "Render/RenderPass/RenderPassBase.h"

class FPreDepthPass final : public FRenderPassBase
{
public:
	FPreDepthPass();
	bool BeginPass(const FPassContext& Ctx) override;
	void EndPass(const FPassContext& Ctx) override;
};
