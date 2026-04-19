#pragma once

#include "Core/CoreTypes.h"
#include "Render/Types/RenderTypes.h"

struct FFrameContext;
class FRenderer;

class FRenderPassContext
{
public:
    explicit FRenderPassContext(FRenderer& InRenderer) : Renderer(InRenderer) {}

    void SubmitRenderPass(ERenderPass Pass);
    void SubmitRenderPassByUserBits(ERenderPass Pass, uint16 UserBits);

    FRenderer& GetRenderer() { return Renderer; }
    const FRenderer& GetRenderer() const { return Renderer; }

private:
    FRenderer& Renderer;
};
