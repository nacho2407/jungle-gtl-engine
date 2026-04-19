#include "Render/Passes/OverlayFontRenderPass.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Core/FrameContext.h"

void FOverlayFontRenderPass::Execute(FRenderer& Renderer, const FFrameContext& Frame)
{
	Renderer.ExecuteOverlayFontRenderPass(Frame);
}
