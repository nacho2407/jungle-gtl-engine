#pragma once

struct FFrameContext;
class FRenderer;

class FRenderPass
{
public:
    virtual ~FRenderPass() = default;
	virtual void Execute(FRenderer& Renderer, const FFrameContext& Frame) = 0;
};
