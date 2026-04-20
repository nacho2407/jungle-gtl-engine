#pragma once

struct FRenderPipelineContext;
class FPrimitiveSceneProxy;

/*
    모든 렌더 패스가 공통으로 따르는 기본 인터페이스입니다.
    입력 준비, 타깃 준비, 드로우 수집, 제출 단계를 패스별로 오버라이드합니다.
*/
class FRenderPass
{
public:
    virtual ~FRenderPass() = default;

    virtual void Reset() {}
    virtual void PrepareInputs(FRenderPipelineContext& Context) = 0;
    virtual void PrepareTargets(FRenderPipelineContext& Context) = 0;
    virtual void BuildDrawCommands(FRenderPipelineContext& Context) = 0;
    virtual void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) = 0;
    virtual void SubmitDrawCommands(FRenderPipelineContext& Context) = 0;

    virtual void Execute(FRenderPipelineContext& Context)
    {
        PrepareInputs(Context);
        PrepareTargets(Context);
        SubmitDrawCommands(Context);
    }
};