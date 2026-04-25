#pragma once

#include "Render/RenderPass/RenderPassBase.h"

class FOpaquePass final : public FRenderPassBase
{
public:
	FOpaquePass();
	void BeginPass(const FPassContext& Ctx) override;
	void EndPass(const FPassContext& Ctx) override;
};
