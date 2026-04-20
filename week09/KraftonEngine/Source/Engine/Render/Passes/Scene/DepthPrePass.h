#pragma once
#include "Render/Passes/RenderPass.h"
struct FFrameContext;
struct FRenderPassContext;
class FPrimitiveSceneProxy;
class FDepthPrePass : public FRenderPass
{
public:
    void PrepareInputs(FRenderPassContext& Context) override;
    void PrepareTargets(FRenderPassContext& Context) override;
    // Depth pre-pass는 개별 프록시 경로만 사용한다.
    void BuildDrawCommands(FRenderPassContext& Context) override { (void)Context; }
    // 현재 Depth pre-pass는 별도 draw command를 만들지 않는다.
    void BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy) override { (void)Context; (void)Proxy; }
    void SubmitDrawCommands(FRenderPassContext& Context) override;
};
