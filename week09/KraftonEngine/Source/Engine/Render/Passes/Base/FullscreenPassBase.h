#pragma once

#include "Render/Passes/Base/RenderPassTypes.h"
#include "Render/Passes/Base/RenderPass.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Submission/Command/DrawCommandList.h"

/*
    화면 전체를 1번 그리는 fullscreen 패스의 공통 베이스 클래스입니다.
    Lighting, Fog, FXAA, Outline 같은 후처리 계열 패스가 이 클래스를 사용합니다.
*/
class FFullscreenPassBase : public FRenderPass
{
public:
    virtual ~FFullscreenPassBase() = default;

    void Execute(FRenderPipelineContext& Context) final
    {
        if (!IsEnabled(Context))
        {
            return;
        }

        PrepareInputs(Context);
        PrepareTargets(Context);
        SubmitDrawCommands(Context);
        Cleanup(Context);
    }

    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy) override
    {
        (void)Context;
        (void)Proxy;
    }

protected:
    virtual bool IsEnabled(const FRenderPipelineContext& Context) const
    {
        (void)Context;
        return true;
    }

    virtual void Cleanup(FRenderPipelineContext& Context)
    {
        (void)Context;
    }

    void BindViewportTarget(FRenderPipelineContext& Context) const
    {
        ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
        ID3D11DepthStencilView* DSV = Context.GetViewportDSV();
        Context.Context->OMSetRenderTargets(1, &RTV, DSV);

        if (Context.StateCache)
        {
            Context.StateCache->RTV = RTV;
            Context.StateCache->DSV = DSV;
        }
    }

    void SubmitPassRange(FRenderPipelineContext& Context, ERenderPass Pass) const
    {
        if (!Context.DrawCommandList)
        {
            return;
        }

        uint32 Start = 0;
        uint32 End = 0;
        Context.DrawCommandList->GetPassRange(Pass, Start, End);
        if (Start < End)
        {
            Context.DrawCommandList->SubmitRange(Start, End, *Context.Device, Context.Context, *Context.StateCache);
        }
    }
};
