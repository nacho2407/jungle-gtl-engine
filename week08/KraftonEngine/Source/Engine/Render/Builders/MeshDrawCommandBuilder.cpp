#include "Render/Builders//MeshDrawCommandBuilder.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/D3D11/Frame/FrameSharedResources.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Scene/PrimitiveSceneProxy.h"
#include "Render/Resource/ConstantBufferPool.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Commands/DrawCommand.h"
#include "Render/Core/PassRenderState.h"
#include "Render/Core/PassTypes.h"
#include "Render/Renderer/Renderer.h"

namespace
{
EPipelineStage ResolvePipelineStage(ERenderPass Pass)
{
    switch (Pass)
    {
    case ERenderPass::Opaque:   return EPipelineStage::BaseDraw;
    case ERenderPass::Decal:    return EPipelineStage::Decal;
    case ERenderPass::Lighting: return EPipelineStage::Lighting;
    default:                    return EPipelineStage::BaseDraw;
    }
}
}

void FMeshDrawCommandBuilder::Build(const FPrimitiveSceneProxy& Proxy, ERenderPass Pass, FRenderPassContext& Context, FDrawCommandList& OutList)
{
    if (!Proxy.MeshBuffer || !Proxy.MeshBuffer->IsValid())
        return;

    ID3D11DeviceContext* Ctx = Context.Context;
    FShader* Shader = Proxy.Shader;

    if (Pass == ERenderPass::DepthPre)
    {
        Shader = FShaderManager::Get().GetShader(EShaderType::SceneDepth);
    }
    else if (Context.ViewModePassRegistry && Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode))
    {
        if (const FRenderPipelinePassDesc* Desc = Context.ViewModePassRegistry->FindPassDesc(Context.ActiveViewMode, ResolvePipelineStage(Pass)))
        {
            if (Desc->CompiledShader)
            {
                Shader = Desc->CompiledShader;
            }
        }
    }

    if (!Shader)
        return;

    const FPassRenderState& PassState = Context.GetPassState(Pass);
    const bool bViewModeBasePass =
        (Pass == ERenderPass::Opaque) &&
        Context.ActiveViewSurfaceSet &&
        Context.ViewModePassRegistry &&
        Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode);

    FConstantBuffer* PerObjCB = Context.Renderer ? Context.Renderer->AcquirePerObjectCBForProxy(Proxy) : nullptr;
    if (!PerObjCB && Context.Resources)
    {
        PerObjCB = &Context.Resources->PerObjectConstantBuffer;
    }

    if (PerObjCB && Ctx && Proxy.NeedsPerObjectCBUpload())
    {
        PerObjCB->Update(Ctx, &Proxy.PerObjectConstants, sizeof(FPerObjectConstants));
        Proxy.ClearPerObjectCBDirty();
    }

    auto AddSection = [&](uint32 FirstIndex, uint32 IndexCount, ID3D11ShaderResourceView* SRV, FConstantBuffer* CB0, FConstantBuffer* CB1)
    {
        if (IndexCount == 0)
            return;

        FDrawCommand& Cmd = OutList.AddCommand();
        Cmd.Shader = Shader;
        Cmd.MeshBuffer = Proxy.MeshBuffer;
        Cmd.FirstIndex = FirstIndex;
        Cmd.IndexCount = IndexCount;

        if (Pass == ERenderPass::DepthPre)
        {
            Cmd.DepthStencil = EDepthStencilState::Default;
            Cmd.Blend = EBlendState::NoColor;
            Cmd.Rasterizer = ERasterizerState::SolidBackCull;
        }
        else
        {
            Cmd.DepthStencil = bViewModeBasePass ? EDepthStencilState::DepthReadOnly
                                                 : ((Pass == ERenderPass::Opaque && Proxy.DepthStencil != EDepthStencilState::Default) ? Proxy.DepthStencil : PassState.DepthStencil);
            Cmd.Blend = (Pass == ERenderPass::Opaque && Proxy.Blend != EBlendState::Opaque) ? Proxy.Blend : PassState.Blend;
            Cmd.Rasterizer = (Pass == ERenderPass::Opaque && Proxy.Rasterizer != ERasterizerState::SolidBackCull) ? Proxy.Rasterizer : PassState.Rasterizer;
        }

        Cmd.Topology = PassState.Topology;
        Cmd.PerObjectCB = PerObjCB;
        Cmd.PerShaderCB[0] = (Pass == ERenderPass::DepthPre) ? nullptr : CB0;
        Cmd.PerShaderCB[1] = (Pass == ERenderPass::DepthPre) ? nullptr : CB1;
        Cmd.DiffuseSRV = (Pass == ERenderPass::DepthPre) ? nullptr : SRV;
        Cmd.Pass = Pass;
        Cmd.SortKey = FDrawCommand::BuildSortKey(Pass, Cmd.Shader, Proxy.MeshBuffer, Cmd.DiffuseSRV);
    };

    if (!Proxy.SectionDraws.empty())
    {
        for (const FMeshSectionDraw& S : Proxy.SectionDraws)
            AddSection(S.FirstIndex, S.IndexCount, S.DiffuseSRV, S.MaterialCB[0], S.MaterialCB[1]);
    }
    else
    {
        AddSection(0, Proxy.MeshBuffer->GetIndexBuffer().GetIndexCount(), Proxy.DiffuseSRV, nullptr, nullptr);
    }
}
