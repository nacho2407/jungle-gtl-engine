#include "Render/Passes/FXAAPass.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Core/FrameContext.h"

void FFXAAPass::Execute(FRenderer& Renderer, const FFrameContext& Frame)
{
	Renderer.ExecuteFXAAPass(Frame);
}
