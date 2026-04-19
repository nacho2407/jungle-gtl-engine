#pragma once

struct FFrameContext;
class FRenderPassContext;

class FRenderPass
{
public:
    virtual ~FRenderPass() = default;
    virtual void Execute(FRenderPassContext& Context, const FFrameContext& Frame) = 0;
};
