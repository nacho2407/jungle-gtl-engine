#pragma once
#include "Render/Passes/RenderPass.h"
struct FSceneView; using FFrameContext = FSceneView;
struct FRenderPassContext;
class FPrimitiveSceneProxy;
class FDecalPass : public FRenderPass
{
public:
    void PrepareInputs(FRenderPassContext& Context) override;
    void PrepareTargets(FRenderPassContext& Context) override;
    // Decal pass는 개별 decal proxy 처리만 담당한다.
    void BuildDrawCommands(FRenderPassContext& Context) override { (void)Context; }
    void BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy) override;
    void SubmitDrawCommands(FRenderPassContext& Context) override;
};
