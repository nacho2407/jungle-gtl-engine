#include "Render/Passes/LightingPass.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Core/FrameContext.h"

void FLightingPass::Execute(FRenderer& Renderer, const FFrameContext& Frame)
{
	Renderer.ExecuteLightingPass(Frame);
}
