#include "Render/Types/PipelineStateTypes.h"
#include "Render/Pipelines/RenderPassTypes.h"
#include "Render/Submission/Builders/DecalDrawCommandBuilder.h"
#include "Render/Submission/Commands/DrawCommandList.h"
#include "Render/Submission/Commands/DrawCommand.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"

void FDecalDrawCommandBuilder::Build(const FPrimitiveSceneProxy& Proxy, FRenderPipelineContext& Context, FDrawCommandList& OutList)
{
    if (!Proxy.DiffuseSRV)
        return;
    FDrawCommand& Cmd = OutList.AddCommand();
    Cmd.Shader = Proxy.Shader;
    Cmd.DepthStencil = EDepthStencilState::NoDepth;
    Cmd.Blend = EBlendState::AlphaBlend;
    Cmd.Rasterizer = ERasterizerState::SolidNoCull;
    Cmd.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    Cmd.VertexCount = 3;
    Cmd.DiffuseSRV = Proxy.DiffuseSRV;
    Cmd.Pass = ERenderPass::Decal;
    Cmd.SortKey = FDrawCommand::BuildSortKey(ERenderPass::Decal, Cmd.Shader, nullptr, Cmd.DiffuseSRV);
}

void FDecalDrawCommandBuilder::BuildReceiver(const FPrimitiveSceneProxy& ReceiverProxy, const FPrimitiveSceneProxy& DecalProxy, FRenderPipelineContext& Context, FDrawCommandList& OutList)
{
    if (!ReceiverProxy.MeshBuffer)
        return;
    FDrawCommand& Cmd = OutList.AddCommand();
    Cmd.Shader = DecalProxy.Shader;
    Cmd.MeshBuffer = ReceiverProxy.MeshBuffer;
    Cmd.IndexCount = ReceiverProxy.MeshBuffer->GetIndexBuffer().GetIndexCount();
    Cmd.Blend = DecalProxy.Blend;
    Cmd.DepthStencil = DecalProxy.DepthStencil;
    Cmd.Rasterizer = DecalProxy.Rasterizer;
    Cmd.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    Cmd.DiffuseSRV = DecalProxy.DiffuseSRV;
    Cmd.Pass = DecalProxy.Pass;
    Cmd.SortKey = FDrawCommand::BuildSortKey(Cmd.Pass, Cmd.Shader, Cmd.MeshBuffer, Cmd.DiffuseSRV);
}
