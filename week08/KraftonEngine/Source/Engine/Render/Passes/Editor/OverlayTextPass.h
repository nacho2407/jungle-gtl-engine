#pragma once
#include "Render/Passes/Base/RenderPass.h"
struct FRenderPipelineContext;
class FPrimitiveSceneProxy;
/*
    오버레이 텍스트 배치를 화면에 합성하는 에디터 패스입니다.
*/
class FOverlayTextPass : public FRenderPass
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    // Overlay text는 텍스트 배치만 소비하므로 프록시 입력을 사용하지 않는다.
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override { (void)Context; (void)Proxy; }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
