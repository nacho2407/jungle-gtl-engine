#pragma once

#include "Render/RenderPass/RenderPassBase.h"
#include "Render/Resource/Buffer.h"

class FEarlyPostProcessPass final : public FRenderPassBase
{
public:
	FEarlyPostProcessPass();
	~FEarlyPostProcessPass() override = default;
	bool BeginPass(const FPassContext& Ctx) override;
	void Execute(const FPassContext& Ctx) override;
	void EndPass(const FPassContext& Ctx) override;

private:
	void EnsureResources(const FPassContext& Ctx);
	void ExecuteDepthOfField(const FPassContext& Ctx);

	FConstantBuffer DOFConstantBuffer;
	bool bResourcesCreated = false;
};
