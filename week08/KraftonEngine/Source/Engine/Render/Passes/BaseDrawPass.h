#pragma once
#include "Render/Core/RenderPass.h"
struct FFrameContext;
class FRenderer;
class FBaseDrawPass : public FRenderPass
{
public:
    void Execute(FRenderer& Renderer, const FFrameContext& Frame) override;
};
