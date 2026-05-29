#pragma once

#include "Render/RenderPass/RenderPassBase.h"

class FEarlyPostProcessPass final : public FRenderPassBase
{
public:
	FEarlyPostProcessPass();
	bool BeginPass(const FPassContext& Ctx) override;
};
