#include "Render/Builders/Mesh/MeshDrawCommandBuilder.h"
#include "Render/Core/PassRenderState.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Scene/PrimitiveSceneProxy.h"
#include "Render/Resource/Pools/ConstantBufferPool.h"
#include "Render/Resource/Managers/ShaderManager.h"
#include "Render/Commands/DrawCommand.h"

void FMeshDrawCommandBuilder::Build(const FPrimitiveSceneProxy& Proxy, ERenderPass Pass, FRenderPassContext& Context, FDrawCommandList& OutList)
{
    if (!Proxy.MeshBuffer || !Proxy.MeshBuffer->IsValid()) return;
    FShader* Shader = Proxy.Shader;
    const FPassRenderState& PassState = Context.GetPassState(Pass);
    auto AddSection = [&](uint32 FirstIndex, uint32 IndexCount, ID3D11ShaderResourceView* SRV, FConstantBuffer* CB0, FConstantBuffer* CB1)
    {
        if (IndexCount == 0) return;
        FDrawCommand& Cmd = OutList.AddCommand();
        Cmd.Shader = Shader; Cmd.MeshBuffer = Proxy.MeshBuffer; Cmd.FirstIndex = FirstIndex; Cmd.IndexCount = IndexCount;
        Cmd.DepthStencil = PassState.DepthStencil; Cmd.Blend = PassState.Blend; Cmd.Rasterizer = PassState.Rasterizer; Cmd.Topology = PassState.Topology;
        Cmd.PerObjectCB = nullptr; Cmd.PerShaderCB[0] = CB0; Cmd.PerShaderCB[1] = CB1; Cmd.DiffuseSRV = SRV; Cmd.Pass = Pass;
        Cmd.SortKey = FDrawCommand::BuildSortKey(Pass, Cmd.Shader, Proxy.MeshBuffer, SRV);
    };
    if (!Proxy.SectionDraws.empty()) { for (const FMeshSectionDraw& S : Proxy.SectionDraws) AddSection(S.FirstIndex,S.IndexCount,S.DiffuseSRV,S.MaterialCB[0],S.MaterialCB[1]); }
    else { AddSection(0, Proxy.MeshBuffer->GetIndexBuffer().GetIndexCount(), Proxy.DiffuseSRV, nullptr, nullptr); }
}
