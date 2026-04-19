#include "Render/Passes/OutlinePass.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Core/FrameContext.h"

void FOutlinePass::Execute(FRenderer& Renderer, const FFrameContext& Frame)
{
	Renderer.ExecuteOutlinePass(Frame);
}
