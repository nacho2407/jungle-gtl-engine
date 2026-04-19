#include "Render/Passes/DepthPrePass.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Core/FrameContext.h"

void FDepthPrePass::Execute(FRenderer& Renderer, const FFrameContext& Frame)
{
	Renderer.ExecuteDepthPrePass(Frame);
}
