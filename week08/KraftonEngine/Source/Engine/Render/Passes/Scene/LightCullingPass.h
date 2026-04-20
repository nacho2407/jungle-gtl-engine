#pragma once
#include "Render/Passes/Base/RenderPass.h"
struct FRenderPipelineContext;
class FPrimitiveSceneProxy;
class FLightCullingPass : public FRenderPass
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override {}
    void BuildDrawCommands(FRenderPipelineContext& Context) override {}
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override {}
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};