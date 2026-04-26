#pragma once

#include "Render/RenderPass/RenderPassBase.h"

class FFXAAPass final : public FRenderPassBase
{
public:
	FFXAAPass();
	bool BeginPass(const FPassContext& Ctx) override;
};
