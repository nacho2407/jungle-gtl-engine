#pragma once

#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Execute/Passes/Base/RenderPass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Submission/Command/DrawCommandList.h"

/*
    Pass Summary
    - Role: shared base for mesh-driven passes that submit draw-command ranges.
    - Inputs: mesh draw commands, viewport RTV/DSV, per-pass render state.
    - Outputs: viewport or pass-specific mesh targets selected by derived classes.
    - Registers: common mesh convention via DrawCommandList.
      VS/PS b0 Frame, VS b1 PerObject, VS/PS b2-b3 PerShader, VS/PS b4 Light,
      PS t0-t2 material textures, PS t6 local lights.
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
        uint32 End   = 0;
        Context.DrawCommandList->GetPassRange(Pass, Start, End);
        if (Start < End)
        {
            Context.DrawCommandList->SubmitRange(Start, End, *Context.Device, Context.Context, *Context.StateCache);
        }
    }
};
