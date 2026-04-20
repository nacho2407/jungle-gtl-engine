#pragma once

class FPrimitiveSceneProxy;
struct FRenderPipelineContext;
class FDrawCommandList;

class FDecalDrawCommandBuilder
{
public:
    static void Build(const FPrimitiveSceneProxy& Proxy, FRenderPipelineContext& Context, FDrawCommandList& OutList);
    static void BuildReceiver(const FPrimitiveSceneProxy& ReceiverProxy, const FPrimitiveSceneProxy& DecalProxy, FRenderPipelineContext& Context, FDrawCommandList& OutList);
};
