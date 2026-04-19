#pragma once
#include "Render/Core/RenderPass.h"
struct FFrameContext;
class FRenderPassContext;
class FDebugLinesPass : public FRenderPass
{
public:
    void Execute(FRenderPassContext& Context, const FFrameContext& Frame) override;
};
