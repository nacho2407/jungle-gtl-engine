#pragma once

#include "Render/RenderPass/RenderPassBase.h"

class FFXAAPass final : public FRenderPassBase
{
public:
	FFXAAPass();
	void BeginPass(const FPassContext& Ctx) override;
};
