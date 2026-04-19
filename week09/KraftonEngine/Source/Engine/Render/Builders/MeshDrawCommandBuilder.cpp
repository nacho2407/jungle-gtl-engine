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
}

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

    auto AddSection = [&](uint32 FirstIndex, uint32 IndexCount, ID3D11ShaderResourceView* SRV, FConstantBuffer* CB0, FConstantBuffer* CB1)
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
            // ViewMode BaseDraw도 실제 scene geometry pass이므로 depth pre-pass에 의존해
            // read-only depth로 강제하면, depth pre가 비었거나 규약이 어긋난 프레임에서
            // 모든 opaque가 깊이 테스트에서 탈락해 화면이 통째로 비어 보일 수 있다.
            // Opaque는 기본적으로 depth write 가능한 기본 상태를 사용한다.
            Cmd.DepthStencil = (Pass == ERenderPass::Opaque && Proxy.DepthStencil != EDepthStencilState::Default)
                                 ? Proxy.DepthStencil
                                 : PassState.DepthStencil;
            Cmd.Blend = (Pass == ERenderPass::Opaque && Proxy.Blend != EBlendState::Opaque) ? Proxy.Blend : PassState.Blend;
            Cmd.Rasterizer = (Pass == ERenderPass::Opaque && Proxy.Rasterizer != ERasterizerState::SolidBackCull) ? Proxy.Rasterizer : PassState.Rasterizer;
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
