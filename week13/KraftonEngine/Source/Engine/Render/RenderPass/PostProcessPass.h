#pragma once

#include "Render/RenderPass/RenderPassBase.h"

class FPostProcessPass final : public FRenderPassBase
{
public:
	FPostProcessPass();
	bool BeginPass(const FPassContext& Ctx) override;
};
