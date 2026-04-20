#pragma once

struct FRenderPipelineContext;
class FDrawCommandList;
class FTextRenderSceneProxy;

class FTextDrawCommandBuilder
{
public:
    static void BuildOverlay(FRenderPipelineContext& Context, FDrawCommandList& OutList);
    static void BuildWorld(const FTextRenderSceneProxy& Proxy, FRenderPipelineContext& Context, FDrawCommandList& OutList);
};
