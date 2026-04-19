#pragma once
#include "Render/Core/RenderPass.h"
struct FFrameContext;
class FRenderer;
class FOutlinePass : public FRenderPass
{
public:
    void Execute(FRenderer& Renderer, const FFrameContext& Frame) override;
};
