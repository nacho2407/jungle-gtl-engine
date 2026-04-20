#pragma once
#include "Render/Passes/RenderPass.h"
struct FSceneView; using FFrameContext = FSceneView;
struct FRenderPassContext;
class FPrimitiveSceneProxy;
class FDebugLinePass : public FRenderPass
{
public:
    void PrepareInputs(FRenderPassContext& Context) override;
    void PrepareTargets(FRenderPassContext& Context) override;
    void BuildDrawCommands(FRenderPassContext& Context) override;
    // Debug line pass는 프록시가 아니라 라인 버퍼만 소비한다.
    void BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy) override { (void)Context; (void)Proxy; }
    void SubmitDrawCommands(FRenderPassContext& Context) override;
};
