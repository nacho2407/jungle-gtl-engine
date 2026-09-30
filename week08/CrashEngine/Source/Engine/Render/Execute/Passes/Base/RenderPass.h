#pragma once

struct FRenderPipelineContext;
class FPrimitiveProxy;

/*
    Pass Summary
    - Role: common interface for all render-pass execution stages.
    - Inputs: FRenderPipelineContext and, when needed, a primitive proxy.
    - Outputs: none directly. Derived passes own all GPU work and render targets.
    - Registers: none directly. Derived passes define concrete bindings.
*/
class FRenderPass
{
public:
    virtual ~FRenderPass() = default;

    virtual void Reset() {}
    virtual void PrepareInputs(FRenderPipelineContext& Context)                                   = 0;
    virtual void PrepareTargets(FRenderPipelineContext& Context)                                  = 0;
    virtual void BuildDrawCommands(FRenderPipelineContext& Context)                               = 0;
    virtual void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) = 0;
    virtual void SubmitDrawCommands(FRenderPipelineContext& Context)                              = 0;

    virtual void Execute(FRenderPipelineContext& Context)
    {
        PrepareInputs(Context);
        PrepareTargets(Context);
        SubmitDrawCommands(Context);
    }
};
