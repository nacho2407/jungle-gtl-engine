#pragma once

#include "Render/Execute/Passes/Base/RenderPassTypes.h"
#include "Render/Execute/Passes/Base/RenderPass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Submission/Command/DrawCommandList.h"

/*
    ���Ͻ� ���� ��ο츦 ó���ϴ� �޽� ��� �н��� ���� ���̽� Ŭ�����Դϴ�.
    Opaque, Decal, Gizmo ���� �޽� �н��� ������ ���� �帧�� �����մϴ�.
*/
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
        uint32 End = 0;
        Context.DrawCommandList->GetPassRange(Pass, Start, End);
        if (Start < End)
        {
            Context.DrawCommandList->SubmitRange(Start, End, *Context.Device, Context.Context, *Context.StateCache);
        }
    }
};
