#pragma once
#include "Render/Execute/Passes/Base/RenderPass.h"
struct FRenderPipelineContext;
class FPrimitiveSceneProxy;
/*
    ?�버�??�인 배치�??�면???�출?�는 ?�디???�스?�니??
*/
class FDebugLinePass : public FRenderPass
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    // Debug line pass???�록?��? ?�니???�인 버퍼�??�비?�다.
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override { (void)Context; (void)Proxy; }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
