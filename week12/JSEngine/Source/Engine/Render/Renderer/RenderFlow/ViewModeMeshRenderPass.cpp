#include "ViewModeMeshRenderPass.h"
#include "Core/Logging/SkinningStats.h"
#include "Core/ResourceManager.h"
#include "Render/Resource/Material.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/ShaderHelper.h"
#include "Render/Resource/ShaderPaths.h"
#include "Render/Resource/VertexFactoryTypes.h"
#include "Render/Scene/RenderBus.h"

namespace
{
    void UpdateFrameBufferWireframeOverride(const FRenderPassContext* Context, bool bWireframe)
    {
        FFrameConstants FrameConstantData = Context->RenderBus->BuildFrameConstants(bWireframe);

        Context->RenderResources->FrameBuffer.Update(
            Context->DeviceContext,
            &FrameConstantData,
            sizeof(FFrameConstants));

        ID3D11Buffer* FrameBuffer = Context->RenderResources->FrameBuffer.GetBuffer();
        Context->DeviceContext->VSSetConstantBuffers(0, 1, &FrameBuffer);
        Context->DeviceContext->PSSetConstantBuffers(0, 1, &FrameBuffer);
    }

    bool IsMeshDebugViewMode(EViewMode ViewMode)
    {
        return ViewMode == EViewMode::Normal ||
            ViewMode == EViewMode::Heatmap ||
            ViewMode == EViewMode::BoneWeightHeatmap;
    }

    FShaderProgram* GetViewModeMeshShaderProgram(const FRenderCommand& Cmd, uint32 PermutationKey)
    {
        if (!Cmd.Material)
        {
            return nullptr;
        }

        const FVertexFactoryDesc& VertexFactoryDesc = FVertexFactoryRegistry::Get(Cmd.VertexFactoryType);

        FShaderStageKey VSKey;
        VSKey.FilePath = FShaderPaths::EditorDebugViewModeMesh;
        VSKey.EntryPoint = VertexFactoryDesc.BasePassVSEntry;
        VSKey.Target = "vs_5_0";
        VSKey.PermutationKey = PermutationKey;

        FShaderStageKey PSKey;
        PSKey.FilePath = FShaderPaths::EditorDebugViewModeMesh;
        PSKey.EntryPoint = "mainPS";
        PSKey.Target = "ps_5_0";
        PSKey.PermutationKey = PermutationKey;

        TArray<D3D_SHADER_MACRO> Macros = FShaderHelper::BuildUberLitMacros(PermutationKey);
        return FResourceManager::Get().GetOrCreateShaderProgram(
            VSKey,
            PSKey,
            Macros.data(),
            Macros.data(),
            &VertexFactoryDesc.VertexLayout);
    }

    void DrawBoundMesh(
        ID3D11DeviceContext* DeviceContext,
        ID3D11Buffer* IndexBuffer,
        uint32 IndexStart,
        uint32 IndexCount,
        uint32 VertexCount)
    {
        if (IndexBuffer != nullptr)
        {
            DeviceContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
            DeviceContext->DrawIndexed(IndexCount, IndexStart, 0);
        }
        else
        {
            DeviceContext->Draw(VertexCount, 0);
        }
    }
}

uint32 FViewModeMeshRenderPass::BuildViewModeMeshPermutationKey(const FRenderPassContext* Context, const UMaterialInterface* Material) const
{
    uint32 PermutationKey = 0;
    PermutationKey |= GetLightingModelPermutationKey(Context->RenderBus->GetViewMode());
    PermutationKey |= GetLightCullingPermutationKey(Context);
    PermutationKey |= GetTexturePermutationKey(Material, true, false, false, true);
    return PermutationKey;
}

bool FViewModeMeshRenderPass::Initialize()
{
    return true;
}

bool FViewModeMeshRenderPass::Release()
{
    return true;
}

bool FViewModeMeshRenderPass::Begin(const FRenderPassContext* Context)
{
    if (!IsMeshDebugViewMode(Context->RenderBus->GetViewMode()))
    {
        OutSRV = PrevPassSRV;
        OutRTV = PrevPassRTV;
        return true;
    }

    const FRenderTargetSet* RenderTargets = Context->RenderTargets;
    ID3D11RenderTargetView* RTV = RenderTargets->DebugViewModeRTV;
    ID3D11DepthStencilView* DSV = RenderTargets->DepthStencilView;

    Context->DeviceContext->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    Context->DeviceContext->OMSetRenderTargets(1, &RTV, DSV);
    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D11BlendState* BlendState = FResourceManager::Get().GetOrCreateBlendState(EBlendType::Opaque);
    Context->DeviceContext->OMSetBlendState(BlendState, nullptr, 0xFFFFFFFF);

    ID3D11DepthStencilState* DepthState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::Default);
    Context->DeviceContext->OMSetDepthStencilState(DepthState, 0);

    FDebugViewModeResolveConstants DebugViewModeResolveConstant = {};
    DebugViewModeResolveConstant.ViewMode = static_cast<uint32>(Context->RenderBus->GetViewMode());
    Context->RenderResources->DebugViewModeResolveConstantBuffer.Update(
        Context->DeviceContext,
        &DebugViewModeResolveConstant,
        sizeof(DebugViewModeResolveConstant));
    ID3D11Buffer* cb7 = Context->RenderResources->DebugViewModeResolveConstantBuffer.GetBuffer();
    Context->DeviceContext->VSSetConstantBuffers(7, 1, &cb7);
    Context->DeviceContext->PSSetConstantBuffers(7, 1, &cb7);

    OutSRV = RenderTargets->DebugViewModeSRV;
    OutRTV = RenderTargets->DebugViewModeRTV;
    return true;
}

bool FViewModeMeshRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    if (!IsMeshDebugViewMode(Context->RenderBus->GetViewMode()))
    {
        return true;
    }

    const TArray<FRenderCommand>& Commands = Context->RenderBus->GetCommands(ERenderPass::ViewModeMesh);
    for (const FRenderCommand& Cmd : Commands)
    {
        if (Cmd.MeshBuffer == nullptr || !Cmd.MeshBuffer->IsValid())
        {
            continue;
        }

        ID3D11Buffer* VertexBuffer = Cmd.MeshBuffer->GetVertexBuffer().GetBuffer();
        const uint32 VertexCount = Cmd.MeshBuffer->GetVertexBuffer().GetVertexCount();
        const uint32 Stride = Cmd.MeshBuffer->GetVertexBuffer().GetStride();
        if (VertexBuffer == nullptr || VertexCount == 0 || Stride == 0)
        {
            continue;
        }

        Context->RenderResources->PerObjectConstantBuffer.Update(
            Context->DeviceContext,
            &Cmd.PerObjectConstants,
            sizeof(FPerObjectConstants));
        ID3D11Buffer* cb1 = Context->RenderResources->PerObjectConstantBuffer.GetBuffer();
        Context->DeviceContext->VSSetConstantBuffers(1, 1, &cb1);
        Context->DeviceContext->PSSetConstantBuffers(1, 1, &cb1);

        if (Cmd.Material)
        {
            const uint32 PermutationKey = BuildViewModeMeshPermutationKey(Context, Cmd.Material);
            FShaderProgram* Program = GetViewModeMeshShaderProgram(Cmd, PermutationKey);
            if (!Program)
            {
                continue;
            }

            Program->Bind(Context->DeviceContext);
            Cmd.Material->BindParameters(Context->DeviceContext, Program->PS);

            BindVertexFactoryResources(
                Context->DeviceContext,
                Cmd.VertexFactoryType,
                Context->RenderBus->GetBoneMatrixConstants(Cmd),
                Context->RenderResources,
                Cmd.BoneMatrixConstantBuffer);

            if (Context->RenderBus->GetViewMode() == EViewMode::BoneWeightHeatmap)
            {
                FBoneWeightHeatmapConstants BoneWeightHeatmapConstants = {};
                BoneWeightHeatmapConstants.SelectedBoneIndex = Cmd.BoneWeightHeatmapBoneIndex;
                BoneWeightHeatmapConstants.bEnabled = Cmd.bUseBoneWeightHeatmap ? 1u : 0u;
                Context->RenderResources->BoneWeightHeatmapConstantBuffer.Update(
                    Context->DeviceContext,
                    &BoneWeightHeatmapConstants,
                    sizeof(FBoneWeightHeatmapConstants));
                ID3D11Buffer* BoneWeightHeatmapBuffer =
                    Context->RenderResources->BoneWeightHeatmapConstantBuffer.GetBuffer();
                Context->DeviceContext->VSSetConstantBuffers(6, 1, &BoneWeightHeatmapBuffer);
                Context->DeviceContext->PSSetConstantBuffers(6, 1, &BoneWeightHeatmapBuffer);
            }
        }

        uint32 Offset = 0;
        Context->DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);

        const bool bGPUSkinnedDraw =
            Cmd.Type == ERenderCommandType::SkeletalMesh && Cmd.bUseBoneMatrixConstants;
        if (bGPUSkinnedDraw)
        {
            FSkinningStats::Get().AddGPUSkinnedDraw(
                Cmd.SkinningWorkVertexCount,
                Cmd.AvgBoneInfluencePerVertex);
        }

        ID3D11Buffer* IndexBuffer = Cmd.MeshBuffer->GetIndexBuffer().GetBuffer();
        DrawBoundMesh(Context->DeviceContext, IndexBuffer, Cmd.SectionIndexStart, Cmd.SectionIndexCount, VertexCount);

        const bool bDrawBoneWeightWireOverlay =
            Context->RenderBus->GetViewMode() == EViewMode::BoneWeightHeatmap &&
            Cmd.Type == ERenderCommandType::SkeletalMesh;
        if (bDrawBoneWeightWireOverlay)
        {
            UpdateFrameBufferWireframeOverride(Context, true);

            ID3D11RasterizerState* PrevRasterizerState = nullptr;
            Context->DeviceContext->RSGetState(&PrevRasterizerState);

            ID3D11RasterizerState* WireRS =
                FResourceManager::Get().GetOrCreateRasterizerState(ERasterizerType::WireFrame);
            Context->DeviceContext->RSSetState(WireRS);

            ID3D11DepthStencilState* DepthReadOnlyState =
                FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::DepthReadOnly);
            Context->DeviceContext->OMSetDepthStencilState(DepthReadOnlyState, 0);

            DrawBoundMesh(Context->DeviceContext, IndexBuffer, Cmd.SectionIndexStart, Cmd.SectionIndexCount, VertexCount);

            UpdateFrameBufferWireframeOverride(Context, false);
            Context->DeviceContext->RSSetState(PrevRasterizerState);
            if (PrevRasterizerState != nullptr)
            {
                PrevRasterizerState->Release();
            }

            ID3D11DepthStencilState* DepthState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::Default);
            Context->DeviceContext->OMSetDepthStencilState(DepthState, 0);
        }
    }

    return true;
}

bool FViewModeMeshRenderPass::End(const FRenderPassContext* Context)
{
    return true;
}
