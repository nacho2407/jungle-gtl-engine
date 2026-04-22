#pragma once
#include "Render/Execute/Passes/Base/RenderPass.h"
struct FRenderPipelineContext;
class FPrimitiveSceneProxy;

/*
    ?�드 그리?��? ?�드 축을 ?�더링하??배경 ?�스?�니??
    Opaque ?�스 ?�전???�행?�어 물체 ?�에 깔리�??�니??
*/
class FGridPass : public FRenderPass
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override { (void)Context; (void)Proxy; }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
