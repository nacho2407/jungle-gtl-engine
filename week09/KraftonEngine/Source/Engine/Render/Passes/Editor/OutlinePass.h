#pragma once
#include "Render/Passes/RenderPass.h"
struct FSceneView; using FFrameContext = FSceneView;
struct FRenderPassContext;
class FPrimitiveSceneProxy;
class FOutlinePass : public FRenderPass
{
public:
    void PrepareInputs(FRenderPassContext& Context) override;
    void PrepareTargets(FRenderPassContext& Context) override;
    void BuildDrawCommands(FRenderPassContext& Context) override;
    // Outline은 fullscreen post-process라서 프록시 입력을 사용하지 않는다.
    void BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy) override { (void)Context; (void)Proxy; }
    void SubmitDrawCommands(FRenderPassContext& Context) override;
};
