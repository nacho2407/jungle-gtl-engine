#include "Render/Passes/GizmoRenderPass.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Core/FrameContext.h"

void FGizmoRenderPass::Execute(FRenderer& Renderer, const FFrameContext& Frame)
{
	Renderer.ExecuteGizmoRenderPass(Frame);
}
