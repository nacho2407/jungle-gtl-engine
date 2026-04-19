#include "Render/Passes/Editor/DebugLinePass.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Builders/Line/LineDrawCommandBuilder.h"
#include "Render/Scene/PrimitiveSceneProxy.h"

void FDebugLinePass::PrepareInputs(FRenderPassContext& Context)
{
    (void)Context;
}

void FDebugLinePass::PrepareTargets(FRenderPassContext& Context)
{
    // simple pass target setup moved into pass-local code.
}

void FDebugLinePass::BuildDrawCommands(FRenderPassContext& Context)
{
    FLineDrawCommandBuilder::Build(Context, *Context.DrawCommandList);
 }

void FDebugLinePass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
        (void)Context; (void)Proxy;
}

void FDebugLinePass::SubmitDrawCommands(FRenderPassContext& Context)
{
        if (Context.DrawCommandList) { uint32 s,e; Context.DrawCommandList->GetPassRange(ERenderPass::EditorLines,s,e); if(s<e) Context.DrawCommandList->SubmitRange(s,e,*Context.Device,Context.Context,*Context.StateCache); }
}
