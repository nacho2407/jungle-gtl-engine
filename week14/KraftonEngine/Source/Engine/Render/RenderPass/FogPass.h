#pragma once

#include "Render/RenderPass/RenderPassBase.h"

class FFogPass final : public FRenderPassBase
{
public:
	FFogPass();
	bool BeginPass(const FPassContext& Ctx) override;
};
