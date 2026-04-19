#include "Render/Passes/BaseDrawPass.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Core/FrameContext.h"

void FBaseDrawPass::Execute(FRenderer& Renderer, const FFrameContext& Frame)
{
	Renderer.ExecuteBaseDrawPass(Frame);
}
