#pragma once

#include "Render/Passes/Base/RenderPass.h"

struct FRenderPipelineContext;
class FPrimitiveSceneProxy;

/*
    최종 viewport 결과를 swapchain backbuffer로 복사하는 프레임 종료 패스입니다.
*/
class FPresentPass : public FRenderPass
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override;
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
