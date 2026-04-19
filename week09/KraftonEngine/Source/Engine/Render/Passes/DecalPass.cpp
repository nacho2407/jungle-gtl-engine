#include "Render/Passes/DecalPass.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Core/FrameContext.h"

void FDecalPass::Execute(FRenderer& Renderer, const FFrameContext& Frame)
{
	Renderer.ExecuteDecalPass(Frame);
}
