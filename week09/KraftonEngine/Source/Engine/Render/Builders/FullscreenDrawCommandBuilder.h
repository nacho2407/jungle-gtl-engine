#pragma once

#include "Render/Core/PassTypes.h"

struct FRenderPassContext;
class FDrawCommandList;

class FFullscreenDrawCommandBuilder
{
public:
    static void Build(ERenderPass Pass, FRenderPassContext& Context, FDrawCommandList& OutList, uint16 UserBits = 0);
};
