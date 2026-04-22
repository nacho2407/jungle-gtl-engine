#pragma once

#include "Render/Execute/Passes/Base/RenderPass.h"

struct FRenderPipelineContext;
class FPrimitiveSceneProxy;

/*
    ���� viewport ����� swapchain backbuffer�� �����ϴ� ������ ���� �н��Դϴ�.
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
