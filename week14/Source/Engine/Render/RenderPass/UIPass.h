#pragma once

#include "Render/RenderPass/RenderPassBase.h"

class FUIPass final : public FRenderPassBase
{
public:
	FUIPass();

	bool BeginPass(const FPassContext& Ctx) override;
	void Execute(const FPassContext& Ctx) override;
};
