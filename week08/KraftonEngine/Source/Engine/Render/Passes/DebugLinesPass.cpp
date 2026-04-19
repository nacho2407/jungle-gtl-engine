#include "Render/Passes/DebugLinesPass.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Core/FrameContext.h"

void FDebugLinesPass::Execute(FRenderer& Renderer, const FFrameContext& Frame)
{
	Renderer.ExecuteDebugLinesPass(Frame);
}
