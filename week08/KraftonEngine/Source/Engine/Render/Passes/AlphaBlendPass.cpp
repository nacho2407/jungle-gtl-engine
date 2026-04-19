#include "Render/Passes/AlphaBlendPass.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Core/FrameContext.h"

void FAlphaBlendPass::Execute(FRenderer& Renderer, const FFrameContext& Frame)
{
	Renderer.ExecuteAlphaBlendPass(Frame);
}
