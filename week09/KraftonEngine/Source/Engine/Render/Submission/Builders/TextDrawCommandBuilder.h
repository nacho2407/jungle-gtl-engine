#pragma once

struct FRenderPassContext;
class FDrawCommandList;
class FTextRenderSceneProxy;

class FTextDrawCommandBuilder
{
public:
    static void BuildOverlay(FRenderPassContext& Context, FDrawCommandList& OutList);
    static void BuildWorld(const FTextRenderSceneProxy& Proxy, FRenderPassContext& Context, FDrawCommandList& OutList);
};
