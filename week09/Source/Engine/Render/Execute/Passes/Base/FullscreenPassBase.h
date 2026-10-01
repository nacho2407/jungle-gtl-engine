#pragma once

#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Execute/Passes/Base/RenderPass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Submission/Command/DrawCommandList.h"

/*
    Pass Summary
    - Role: shared base for fullscreen passes that submit fullscreen draw ranges.
    - Inputs: fullscreen draw commands, viewport RTV/DSV, pass constants and SRVs.
    - Outputs: viewport or pass-specific fullscreen targets.
    - Registers: common fullscreen convention via DrawCommandList.
      VS/PS b0 Frame, optional PS b2-b3 PerShader, optional PS t0+ SRVs.
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

    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override
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

    // 지정한 Render Pass에 속한 draw command 구간만 DrawCommandList에서 찾아 제출한다.
    // command를 새로 생성하지 않으며, 이미 Build 단계에서 만들어진 [Start, End) 범위의 command를 실제 draw call로 실행한다.
    void SubmitPassRange(FRenderPipelineContext& Context, ERenderPass Pass) const
    {
        if (!Context.DrawCommandList)
        {
            return;
        }

        uint32 Start = 0;
        uint32 End   = 0;
        Context.DrawCommandList->GetPassRange(Pass, Start, End);
        if (Start < End)
        {
            Context.DrawCommandList->SubmitRange(Start, End, *Context.Device, Context.Context, *Context.StateCache);
        }
    }
};
