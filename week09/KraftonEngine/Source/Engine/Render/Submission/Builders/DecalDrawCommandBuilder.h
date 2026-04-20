#pragma once

class FPrimitiveSceneProxy;
struct FRenderPassContext;
class FDrawCommandList;

class FDecalDrawCommandBuilder
{
public:
    static void Build(const FPrimitiveSceneProxy& Proxy, FRenderPassContext& Context, FDrawCommandList& OutList);
    static void BuildReceiver(const FPrimitiveSceneProxy& ReceiverProxy, const FPrimitiveSceneProxy& DecalProxy, FRenderPassContext& Context, FDrawCommandList& OutList);
};
