// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Execute/Passes/Base/RenderPass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Submission/Command/DrawCommandList.h"

// FMeshPassBase는 메시 데이터와 렌더 제출 정보를 다룹니다.
class FMeshPassBase : public FRenderPass
{
public:
    virtual ~FMeshPassBase() = default;

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

    void BuildDrawCommands(FRenderPipelineContext& Context) override
    {
        (void)Context;
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
        uint32 End   = 0;
        Context.DrawCommandList->GetPassRange(Pass, Start, End);
        if (Start < End)
        {
            Context.DrawCommandList->SubmitRange(Start, End, *Context.Device, Context.Context, *Context.StateCache);
        }
    }
};
