#include "Render/Passes/Editor/OutlinePass.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Builders/Fullscreen/FullscreenDrawCommandBuilder.h"
#include "Render/Scene/PrimitiveSceneProxy.h"

void FOutlinePass::PrepareInputs(FRenderPassContext& Context)
{
    (void)Context;
}

void FOutlinePass::PrepareTargets(FRenderPassContext& Context)
{
    // post process target setup moved into pass-local code.
}

void FOutlinePass::BuildDrawCommands(FRenderPassContext& Context)
{
    FFullscreenDrawCommandBuilder::Build(ERenderPass::PostProcess, Context, *Context.DrawCommandList, 1);
 }

void FOutlinePass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
        (void)Context; (void)Proxy;
}

void FOutlinePass::SubmitDrawCommands(FRenderPassContext& Context)
{
        if (Context.DrawCommandList) { uint32 s,e; Context.DrawCommandList->GetPassRange(ERenderPass::PostProcess,s,e); for(uint32 i=s;i<e;++i){ const auto& c=Context.DrawCommandList->GetCommands()[i]; if((c.SortKey & 0xFFFu)==1) Context.DrawCommandList->SubmitRange(i,i+1,*Context.Device,Context.Context,*Context.StateCache);} }
}
