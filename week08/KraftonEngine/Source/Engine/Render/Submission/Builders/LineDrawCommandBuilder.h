#pragma once

struct FRenderPipelineContext;
class FDrawCommandList;

class FLineDrawCommandBuilder
{
public:
    static void Build(FRenderPipelineContext& Context, FDrawCommandList& OutList);
};
