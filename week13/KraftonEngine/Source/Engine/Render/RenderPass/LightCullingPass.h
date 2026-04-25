#pragma once

#include "Render/RenderPass/RenderPassBase.h"

class FLightCullingPass final : public FRenderPassBase
{
public:
	FLightCullingPass();
	void Execute(const FPassContext& Ctx) override;
};
