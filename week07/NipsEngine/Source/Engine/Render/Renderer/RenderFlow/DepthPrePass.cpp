#include "DepthPrePass.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/Material.h"

bool FDepthPrePass::Initialize()
{
	return true;
}

bool FDepthPrePass::Release()
{
	return true;
}

bool FDepthPrePass::Begin(const FRenderPassContext* Context)
{
	return true;
}

bool FDepthPrePass::DrawCommand(const FRenderPassContext* Context)
{
	return true;
}

bool FDepthPrePass::End(const FRenderPassContext* Context)
{
	return true;
}
