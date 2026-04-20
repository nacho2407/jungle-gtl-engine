#pragma once
#include "Render/Passes/RenderPass.h"
struct FFrameContext;
struct FRenderPassContext;
class FPrimitiveSceneProxy;
class FSelectionMaskPass : public FRenderPass
{
public:
    void PrepareInputs(FRenderPassContext& Context) override;
    void PrepareTargets(FRenderPassContext& Context) override;
    // Selection mask는 선택된 프록시 경로에서만 커맨드를 만든다.
    void BuildDrawCommands(FRenderPassContext& Context) override { (void)Context; }
    void BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy) override;
    void SubmitDrawCommands(FRenderPassContext& Context) override;
};
