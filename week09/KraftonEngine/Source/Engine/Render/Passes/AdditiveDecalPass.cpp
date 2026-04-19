#include "Render/Passes/AdditiveDecalPass.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Core/FrameContext.h"

void FAdditiveDecalPass::Execute(FRenderer& Renderer, const FFrameContext& Frame)
{
	Renderer.ExecuteAdditiveDecalPass(Frame);
}
