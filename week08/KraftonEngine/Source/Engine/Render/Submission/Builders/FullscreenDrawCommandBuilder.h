#pragma once

#include "Render/Pipelines/ViewMode/ViewModePassConfig.h"

struct FRenderPassContext;
class FDrawCommandList;

class FFullscreenDrawCommandBuilder
{
public:
    static void Build(ERenderPass Pass, FRenderPassContext& Context, FDrawCommandList& OutList, uint16 UserBits = 0);
};
