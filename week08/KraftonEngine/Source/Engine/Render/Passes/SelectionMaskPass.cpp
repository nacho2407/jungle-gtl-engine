#include "Render/Passes/SelectionMaskPass.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Core/FrameContext.h"

void FSelectionMaskPass::Execute(FRenderer& Renderer, const FFrameContext& Frame)
{
	Renderer.ExecuteSelectionMaskPass(Frame);
}
