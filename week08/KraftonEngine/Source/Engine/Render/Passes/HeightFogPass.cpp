#include "Render/Passes/HeightFogPass.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Core/FrameContext.h"

void FHeightFogPass::Execute(FRenderer& Renderer, const FFrameContext& Frame)
{
	Renderer.ExecuteHeightFogPass(Frame);
}
