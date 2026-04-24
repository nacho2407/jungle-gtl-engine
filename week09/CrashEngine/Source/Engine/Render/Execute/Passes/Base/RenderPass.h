// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

struct FRenderPipelineContext;
class FPrimitiveProxy;

// FRenderPass는 렌더 파이프라인의 한 실행 단계를 담당합니다.
class FRenderPass
{
public:
    virtual ~FRenderPass() = default;

    virtual void Reset() {}
    virtual void PrepareInputs(FRenderPipelineContext& Context)                                        = 0;
    virtual void PrepareTargets(FRenderPipelineContext& Context)                                       = 0;
    virtual void BuildDrawCommands(FRenderPipelineContext& Context)                                    = 0;
    virtual void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) = 0;
    virtual void SubmitDrawCommands(FRenderPipelineContext& Context)                                   = 0;

    virtual void Execute(FRenderPipelineContext& Context)
    {
        PrepareInputs(Context);
        PrepareTargets(Context);
        SubmitDrawCommands(Context);
    }
};

