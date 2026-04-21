#pragma once
#include "Render/Passes/Base/RenderPass.h"
struct FRenderPipelineContext;
class FPrimitiveSceneProxy;

/*
    월드 그리드와 월드 축을 렌더링하는 배경 패스입니다.
    Opaque 패스 이전에 실행되어 물체 뒤에 깔리게 합니다.
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
