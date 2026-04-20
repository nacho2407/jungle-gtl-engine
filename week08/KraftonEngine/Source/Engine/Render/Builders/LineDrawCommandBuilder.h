#pragma once

struct FRenderPassContext;
class FDrawCommandList;

class FLineDrawCommandBuilder
{
public:
    static void Build(FRenderPassContext& Context, FDrawCommandList& OutList);
};
