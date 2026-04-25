#pragma once

#include "Render/RenderPass/RenderPassBase.h"

class FPostProcessPass final : public FRenderPassBase
{
public:
	FPostProcessPass();
	void BeginPass(const FPassContext& Ctx) override;
};
