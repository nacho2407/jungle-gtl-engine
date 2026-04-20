#include "Render/Submission/Builders/MeshDrawCommandBuilder.h"
#include "Render/Passes/Common/RenderPassContext.h"
#include "Render/Resources/Frame/FrameSharedResources.h"
#include "Render/Submission/Commands/DrawCommandList.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Resources/Pools/ConstantBufferPool.h"
#include "Render/Resources/Managers/ShaderManager.h"
#include "Render/Submission/Commands/DrawCommand.h"
#include "Render/Passes/Common/PassRenderState.h"
#include "Render/Pipelines/ViewMode/ViewModePassConfig.h"
#include "Render/Execution/Renderer.h"


namespace
{
bool TryResolveViewModeStage(ERenderPass Pass, EPipelineStage& OutStage)
{
    switch (Pass)
    {
    case ERenderPass::Opaque:
        OutStage = EPipelineStage::BaseDraw;
        return true;
    case ERenderPass::Decal:
        OutStage = EPipelineStage::Decal;
        return true;
    case ERenderPass::Lighting:
        OutStage = EPipelineStage::Lighting;
        return true;
    default:
        return false;
    }
}
} // namespace

void FMeshDrawCommandBuilder::Build(const FPrimitiveSceneProxy& Proxy, ERenderPass Pass, FRenderPassContext& Context, FDrawCommandList& OutList)
{
    const bool bHasMeshBuffer = (Proxy.MeshBuffer != nullptr);
    const bool bMeshValid = bHasMeshBuffer && Proxy.MeshBuffer->IsValid();

    if (!bMeshValid)
    {
        return;
    }

    ID3D11DeviceContext* Ctx = Context.Context;
    FShader* Shader = Proxy.Shader;

    if (Pass == ERenderPass::DepthPre)
    {
        Shader = FShaderManager::Get().GetShader(EShaderType::DepthOnly);
    }
    else if (Context.ViewModePassRegistry && Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode))
    {
        EPipelineStage ViewModeStage;
        const bool bSupportsViewModeStage = TryResolveViewModeStage(Pass, ViewModeStage);
        const bool bRequiresViewModeSurface = (Pass == ERenderPass::Opaque || Pass == ERenderPass::Decal);

        if (bSupportsViewModeStage && (!bRequiresViewModeSurface || Context.ActiveViewSurfaceSet))
        {
            if (const FRenderPipelinePassDesc* Desc = Context.ViewModePassRegistry->FindPassDesc(Context.ActiveViewMode, ViewModeStage))
            {
                if (Desc->CompiledShader)
                {
                    Shader = Desc->CompiledShader;
                }
            }
        }
    }

    if (!Shader)
    {
        return;
    }

    const FPassRenderState& PassState = Context.GetPassState(Pass);

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

    FConstantBuffer* ExtraCB0 = nullptr;
    FConstantBuffer* ExtraCB1 = nullptr;
    if (Pass != ERenderPass::DepthPre && Proxy.ExtraCB.Buffer && Proxy.ExtraCB.Size > 0 && Ctx)
    {
        Proxy.ExtraCB.Buffer->Update(Ctx, Proxy.ExtraCB.Data, Proxy.ExtraCB.Size);

        if (Proxy.ExtraCB.Slot == ECBSlot::PerShader0)
        {
            ExtraCB0 = Proxy.ExtraCB.Buffer;
        }
        else if (Proxy.ExtraCB.Slot == ECBSlot::PerShader1)
        {
            ExtraCB1 = Proxy.ExtraCB.Buffer;
        }
    }

    auto AddSection = [&](uint32 FirstIndex, uint32 IndexCount, ID3D11ShaderResourceView* BaseSRV, ID3D11ShaderResourceView* InNormalSRV,
                          FConstantBuffer* CB0, FConstantBuffer* CB1,
                          EBlendState SectionBlend, EDepthStencilState SectionDepthStencil, ERasterizerState SectionRasterizer)
    {
        if (IndexCount == 0)
        {
            return;
        }

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
            const bool bUsesViewModeBaseDraw =
                (Pass == ERenderPass::Opaque) &&
                Context.ViewModePassRegistry &&
                Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode);

            // Reversed-Z depth pre-pass writes the same geometry depth first.
            // BaseDraw must then use GREATER_EQUAL-style read-only depth or every
            // opaque pixel at the exact same depth fails the comparison.
            if (Pass == ERenderPass::Opaque && SectionDepthStencil != EDepthStencilState::Default)
            {
                Cmd.DepthStencil = SectionDepthStencil;
            }
            else if (bUsesViewModeBaseDraw)
            {
                Cmd.DepthStencil = EDepthStencilState::DepthReadOnly;
            }
            else
            {
                Cmd.DepthStencil = PassState.DepthStencil;
            }
            Cmd.Blend = (Pass == ERenderPass::Opaque && SectionBlend != EBlendState::Opaque) ? SectionBlend : PassState.Blend;
            Cmd.Rasterizer = (Pass == ERenderPass::Opaque && SectionRasterizer != ERasterizerState::SolidBackCull) ? SectionRasterizer : PassState.Rasterizer;
        }

        if (Pass == ERenderPass::Opaque &&
            Context.ViewModePassRegistry &&
            Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode) &&
            Context.ViewModePassRegistry->ShouldForceWireframeRasterizer(Context.ActiveViewMode))
        {
            Cmd.Rasterizer = ERasterizerState::WireFrame;
        }

        Cmd.Topology = PassState.Topology;
        Cmd.PerObjectCB = PerObjCB;
        Cmd.PerShaderCB[0] = (Pass == ERenderPass::DepthPre) ? nullptr : (CB0 ? CB0 : ExtraCB0);
        Cmd.PerShaderCB[1] = (Pass == ERenderPass::DepthPre) ? nullptr : (CB1 ? CB1 : ExtraCB1);
        Cmd.DiffuseSRV = (Pass == ERenderPass::DepthPre) ? nullptr : BaseSRV;
        Cmd.NormalSRV = (Pass == ERenderPass::DepthPre) ? nullptr : InNormalSRV;
        Cmd.Pass = Pass;
        const uintptr_t MaterialHash =
            (reinterpret_cast<uintptr_t>(Cmd.PerShaderCB[0]) >> 4) ^
            (reinterpret_cast<uintptr_t>(Cmd.PerShaderCB[1]) >> 9) ^
            (reinterpret_cast<uintptr_t>(Cmd.DiffuseSRV) >> 14) ^
            (reinterpret_cast<uintptr_t>(Cmd.NormalSRV) >> 19);
        Cmd.SortKey = FDrawCommand::BuildSortKey(
            Pass,
            Cmd.Shader,
            Proxy.MeshBuffer,
            Cmd.DiffuseSRV,
            static_cast<uint16>(MaterialHash & 0x0FFFu));
    };

    if (!Proxy.SectionDraws.empty())
    {
        for (const FMeshSectionDraw& S : Proxy.SectionDraws)
        {
            AddSection(
                S.FirstIndex,
                S.IndexCount,
                S.DiffuseSRV,
                S.NormalSRV,
                S.MaterialCB[0],
                S.MaterialCB[1],
                S.Blend,
                S.DepthStencil,
                S.Rasterizer);
        }
    }
    else
    {
        AddSection(
            0,
            Proxy.MeshBuffer->GetIndexBuffer().GetIndexCount(),
            Proxy.DiffuseSRV,
            nullptr,
            nullptr,
            nullptr,
            Proxy.Blend,
            Proxy.DepthStencil,
            Proxy.Rasterizer);
    }
}
