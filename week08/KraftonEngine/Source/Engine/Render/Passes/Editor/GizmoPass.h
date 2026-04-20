#pragma once
#include "Render/Passes/RenderPass.h"
struct FFrameContext;
struct FRenderPassContext;
class FPrimitiveSceneProxy;
class FGizmoPass : public FRenderPass
{
public:
    void PrepareInputs(FRenderPassContext& Context) override;
    void PrepareTargets(FRenderPassContext& Context) override;
    // Gizmo pass는 gizmo proxy만 개별적으로 처리한다.
    void BuildDrawCommands(FRenderPassContext& Context) override { (void)Context; }
    void BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy) override;
    void SubmitDrawCommands(FRenderPassContext& Context) override;
};
