#pragma once

struct FRenderPipelineContext;
class FDrawCommandList;

class FLineDrawCommandBuilder
{
public:
    static void BuildGrid(FRenderPipelineContext& Context, FDrawCommandList& OutList);
    static void BuildDebugLines(FRenderPipelineContext& Context, FDrawCommandList& OutList);
};
