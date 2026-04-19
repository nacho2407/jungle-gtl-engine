#pragma once
#include "Render/Passes/RenderPass.h"
struct FFrameContext;
struct FRenderPassContext;
class FPrimitiveSceneProxy;
class FAlphaBlendPass : public FRenderPass
{
public:
    void PrepareInputs(FRenderPassContext& Context) override;
    void PrepareTargets(FRenderPassContext& Context) override;
    void BuildDrawCommands(FRenderPassContext& Context) override;
    void BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy) override;
    void SubmitDrawCommands(FRenderPassContext& Context) override;
};
