#include "Render/Passes/Scene/DepthPrePass.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Scene/PrimitiveSceneProxy.h"

void FDepthPrePass::PrepareInputs(FRenderPassContext& Context)
{
    (void)Context;
}

void FDepthPrePass::PrepareTargets(FRenderPassContext& Context)
{
    // simple pass target setup moved into pass-local code.
}

void FDepthPrePass::BuildDrawCommands(FRenderPassContext& Context)
{
    (void)Context;
 }

void FDepthPrePass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
        (void)Context; (void)Proxy;
}

void FDepthPrePass::SubmitDrawCommands(FRenderPassContext& Context)
{
        (void)Context;
}
