#include "Render/Core/RenderPipeline.h"

#include "Render/Renderer/Renderer.h"

void ExecuteRenderPipeline(FRenderer& Renderer, ERenderPipelineType Type, const FFrameContext& Frame)
{
    Renderer.RunRootPipeline(Type, Frame);
}
