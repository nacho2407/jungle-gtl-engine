#pragma once

#include "Render/RenderPass/RenderPassBase.h"

class FGammaCorrectionPass final : public FRenderPassBase
{
public:
	FGammaCorrectionPass();
	bool BeginPass(const FPassContext& Ctx) override;
	void EndPass(const FPassContext& Ctx) override;
};
