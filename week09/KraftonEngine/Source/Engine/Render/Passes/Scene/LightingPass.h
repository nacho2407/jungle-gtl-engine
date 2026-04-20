#pragma once
#include "Render/Passes/Base/FullscreenPassBase.h"
struct FRenderPipelineContext;
class FPrimitiveSceneProxy;
/*
    뷰 모드 표면을 읽어 최종 조명 결과를 합성하는 fullscreen 패스입니다.
*/
class FLightingPass : public FFullscreenPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void PrepareTargets(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    // Lighting은 fullscreen 합성 pass라서 프록시 입력을 사용하지 않는다.
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override { (void)Context; (void)Proxy; }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};
