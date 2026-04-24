// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include "Render/Execute/Passes/Base/PostProcessPassBase.h"
struct FRenderPipelineContext;
class FPrimitiveProxy;
// FOutlinePass는 렌더 파이프라인의 한 실행 단계를 담당합니다.
class FOutlinePass : public FPostProcessPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override
    {
        (void)Context;
        (void)Proxy;
    }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
};

