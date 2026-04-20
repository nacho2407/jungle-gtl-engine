#pragma once

struct FSceneView; using FFrameContext = FSceneView;
struct FRenderPassContext;
class FPrimitiveSceneProxy;

class FRenderPass
{
public:
    virtual ~FRenderPass() = default;
    virtual void Reset() {}
    virtual void PrepareInputs(FRenderPassContext& Context) = 0;
    virtual void PrepareTargets(FRenderPassContext& Context) = 0;
    virtual void BuildDrawCommands(FRenderPassContext& Context) = 0;
    virtual void BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy) = 0;
    virtual void SubmitDrawCommands(FRenderPassContext& Context) = 0;

    virtual void Execute(FRenderPassContext& Context)
    {
        PrepareInputs(Context);
        PrepareTargets(Context);
        SubmitDrawCommands(Context);
    }
};
