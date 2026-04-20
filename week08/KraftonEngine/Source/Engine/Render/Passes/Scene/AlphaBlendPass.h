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
    // Alpha blend pass는 프록시 단위 submit만 사용한다.
    void BuildDrawCommands(FRenderPassContext& Context) override { (void)Context; }
    void BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy) override;
    void SubmitDrawCommands(FRenderPassContext& Context) override;
};
