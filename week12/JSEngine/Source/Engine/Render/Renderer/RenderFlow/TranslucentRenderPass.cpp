#include "TranslucentRenderPass.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/Material.h"
#include "Render/Resource/ShaderHelper.h"
#include "Render/Resource/VertexFactoryTypes.h"
#include "Core/ResourceManager.h"

#include <algorithm>

namespace
{
    FShaderProgram* GetShaderProgram(const FRenderCommand& Cmd, uint32 PermutationKey)
    {
        if (!Cmd.Material)
        {
            return nullptr;
        }
        if (Cmd.Material->GetShaderType() == EMaterialShaderType::None)
        {
            UE_LOG_WARNING("[Render] ShaderType None material cannot be drawn by TranslucentRenderPass: %s", Cmd.Material->GetName().c_str());
            return nullptr;
        }

        const FVertexFactoryDesc& VertexFactoryDesc = FVertexFactoryRegistry::Get(Cmd.VertexFactoryType);

        FShaderStageKey VSKey;
        VSKey.FilePath = VertexFactoryDesc.VertexShaderPath;
        VSKey.EntryPoint = VertexFactoryDesc.BasePassVSEntry;
        VSKey.Target = "vs_5_0";
        VSKey.PermutationKey = PermutationKey;

        FShaderStageKey PSKey;
        PSKey.FilePath = Cmd.Material->GetPixelShaderPath();
        PSKey.EntryPoint = Cmd.Material->GetPixelShaderEntryPoint();
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

    FShaderProgram* GetParticleSpriteShaderProgram()
    {
        const FVertexFactoryDesc& ParticleSpriteDesc = FVertexFactoryRegistry::Get(EVertexFactoryType::ParticleSprite);

        FShaderStageKey VSKey;
        VSKey.FilePath = ParticleSpriteDesc.VertexShaderPath;
        VSKey.EntryPoint = ParticleSpriteDesc.BasePassVSEntry;
        VSKey.Target = "vs_5_0";

        FShaderStageKey PSKey;
        PSKey.FilePath = FShaderPaths::VFXParticle;
        PSKey.EntryPoint = "PS";
        PSKey.Target = "ps_5_0";

        return FResourceManager::Get().GetOrCreateShaderProgram(
            VSKey,
            PSKey,
            nullptr,
            nullptr,
            &ParticleSpriteDesc.VertexLayout);
    }

    bool IsParticleSpriteCommand(const FRenderCommand& Cmd)
    {
        return Cmd.Type == ERenderCommandType::Particle
            && Cmd.VertexFactoryType == EVertexFactoryType::ParticleSprite
            && Cmd.InstanceBufferView.IsValid();
    }

    bool IsParticleMeshCommand(const FRenderCommand& Cmd)
    {
        return Cmd.Type == ERenderCommandType::Particle
            && Cmd.VertexFactoryType == EVertexFactoryType::ParticleMesh
            && Cmd.InstanceBufferView.IsValid();
    }

    float CalculateTranslucentSortKey(
        const FRenderCommand& Cmd,
        const FVector& CameraPosition,
        const FVector& CameraForward)
    {
        const FVector Center = Cmd.WorldAABB.IsValid()
            ? Cmd.WorldAABB.GetCenter()
            : Cmd.PerObjectConstants.Model.GetOrigin();
        const FVector Delta = Center - CameraPosition;
        return FVector::DotProduct(Delta, CameraForward);
    }
} // namespace

bool FTranslucentRenderPass::Initialize()
{
    return true;
}

bool FTranslucentRenderPass::Release()
{
    return true;
}

bool FTranslucentRenderPass::Begin(const FRenderPassContext* Context)
{
    if (IsDebugViewMode(Context->RenderBus->GetViewMode()))
    {
        OutSRV = PrevPassSRV;
        OutRTV = PrevPassRTV;
        return true;
    }

    ID3D11RenderTargetView* RTV = PrevPassRTV;
    ID3D11DepthStencilView* DSV = Context->RenderTargets->DepthStencilView;
    Context->DeviceContext->OMSetRenderTargets(1, &RTV, DSV);
    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    OutSRV = PrevPassSRV;
    OutRTV = PrevPassRTV;

    return true;
}

bool FTranslucentRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    // 현재 Translucent가 렌더링되어야 하는 DebugViewMode 없음
    if (IsDebugViewMode(Context->RenderBus->GetViewMode()))
    {
        return true;
    }
    
    const TArray<FRenderCommand> Commands = SortTranslucentCommands(Context);
    if (Commands.empty())
    {
        return true;
    }

    for (const FRenderCommand& Cmd : Commands)
    {
        if (!DrawEachCommand(Context, Cmd)) 
            continue;        // Draw 실패 시 동작 추가할 거면 여기에 추가
    }

    return true;
}

bool FTranslucentRenderPass::End(const FRenderPassContext* Context)
{
    return true;
}

TArray<FRenderCommand> FTranslucentRenderPass::SortTranslucentCommands(const FRenderPassContext* Context)
{
    const TArray<FRenderCommand>& Commands = Context->RenderBus->GetCommands(ERenderPass::Translucent);
    TArray<FRenderCommand> SortedCommands = Commands;
    const FVector CameraPosition = Context->RenderBus->GetCameraPosition();
    const FVector CameraForward = Context->RenderBus->GetCameraForward();
    std::sort(SortedCommands.begin(), SortedCommands.end(),
        [&CameraPosition, &CameraForward](const FRenderCommand& A, const FRenderCommand& B)
        {
            return CalculateTranslucentSortKey(A, CameraPosition, CameraForward) >
                CalculateTranslucentSortKey(B, CameraPosition, CameraForward);
        });
    return SortedCommands;
}

bool FTranslucentRenderPass::DrawEachCommand(const FRenderPassContext* Context, const FRenderCommand& Cmd)
{
    ID3D11DeviceContext* DeviceContext = Context->DeviceContext;
    
    Context->RenderResources->PerObjectConstantBuffer.Update(
        DeviceContext,
        &Cmd.PerObjectConstants,
        sizeof(FPerObjectConstants));
    ID3D11Buffer* cb1 = Context->RenderResources->PerObjectConstantBuffer.GetBuffer();
    DeviceContext->VSSetConstantBuffers(1, 1, &cb1);
    DeviceContext->PSSetConstantBuffers(1, 1, &cb1);

    if (IsParticleSpriteCommand(Cmd))
    {
        return DrawParticleSpriteCommand(Context, Cmd);
    }

    if (IsParticleMeshCommand(Cmd))
    {
        return DrawParticleMeshCommand(Context, Cmd);
    }

    if (Cmd.MeshBuffer == nullptr || !Cmd.MeshBuffer->IsValid())
    {
        return false;
    }

    uint32 offset = 0;
    ID3D11Buffer* vertexBuffer = Cmd.MeshBuffer->GetVertexBuffer().GetBuffer();
    if (vertexBuffer == nullptr)
    {
        return false;
    }

    uint32 vertexCount = Cmd.MeshBuffer->GetVertexBuffer().GetVertexCount();
    uint32 stride = Cmd.MeshBuffer->GetVertexBuffer().GetStride();
    if (vertexCount == 0 || stride == 0)
    {
        return false;
    }

    if (Cmd.Material != nullptr)
    {
        uint32 PermutationKey = 0;
        PermutationKey |= GetLightingModelPermutationKey(Context->RenderBus->GetViewMode());
        PermutationKey |= GetTexturePermutationKey(Cmd.Material);
        
        FShaderProgram* Program = GetShaderProgram(Cmd, PermutationKey);
        if (!Program)
        {
            return false;
        }

        Program->Bind(DeviceContext);
        Cmd.Material->BindRenderStates(DeviceContext);
        Cmd.Material->BindParameters(DeviceContext, Program->PS);
        BindVertexFactoryResources(
            DeviceContext,
            Cmd.VertexFactoryType,
            Context->RenderBus->GetBoneMatrixConstants(Cmd),
            Context->RenderResources,
            Cmd.BoneMatrixConstantBuffer);
    }

    // 최종 Draw
    DeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

    ID3D11Buffer* indexBuffer = Cmd.MeshBuffer->GetIndexBuffer().GetBuffer();
    if (indexBuffer != nullptr)
    {
        DeviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        DeviceContext->DrawIndexed(Cmd.SectionIndexCount, Cmd.SectionIndexStart, 0);
    }
    else
    {
        DeviceContext->Draw(vertexCount, 0);
    }
    return true;
}

bool FTranslucentRenderPass::DrawParticleMeshCommand(const FRenderPassContext* Context, const FRenderCommand& Cmd)
{
    ID3D11DeviceContext* DeviceContext = Context->DeviceContext;
    if (Cmd.MeshBuffer == nullptr || !Cmd.MeshBuffer->IsValid())
    {
        return false;
    }

    ID3D11Buffer* VertexBuffer = Cmd.MeshBuffer->GetVertexBuffer().GetBuffer();
    ID3D11Buffer* IndexBuffer = Cmd.MeshBuffer->GetIndexBuffer().GetBuffer();
    if (VertexBuffer == nullptr || IndexBuffer == nullptr)
    {
        return false;
    }

    const uint32 VertexStride = Cmd.MeshBuffer->GetVertexBuffer().GetStride();
    if (Cmd.MeshBuffer->GetVertexBuffer().GetVertexCount() == 0 || VertexStride == 0 || Cmd.SectionIndexCount == 0)
    {
        return false;
    }

    if (Cmd.Material != nullptr)
    {
        uint32 PermutationKey = 0;
        PermutationKey |= GetLightingModelPermutationKey(Context->RenderBus->GetViewMode());
        PermutationKey |= GetTexturePermutationKey(Cmd.Material);

        FShaderProgram* Program = GetShaderProgram(Cmd, PermutationKey);
        if (Program == nullptr)
        {
            return false;
        }

        Program->Bind(DeviceContext);
        Cmd.Material->BindRenderStates(DeviceContext);
        Cmd.Material->BindParameters(DeviceContext, Program->PS);
        BindVertexFactoryResources(
            DeviceContext,
            Cmd.VertexFactoryType,
            Context->RenderBus->GetBoneMatrixConstants(Cmd),
            Context->RenderResources,
            Cmd.BoneMatrixConstantBuffer);
    }

    ID3D11DepthStencilState* DepthState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::DepthReadOnly);
    DeviceContext->OMSetDepthStencilState(DepthState, 0);

    ID3D11Buffer* VertexBuffers[2] = { VertexBuffer, Cmd.InstanceBufferView.Buffer };
    uint32 Strides[2] = { VertexStride, Cmd.InstanceBufferView.Stride };
    uint32 Offsets[2] = { 0, Cmd.InstanceBufferView.Offset };
    DeviceContext->IASetVertexBuffers(0, 2, VertexBuffers, Strides, Offsets);
    DeviceContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    DeviceContext->DrawIndexedInstanced(Cmd.SectionIndexCount, Cmd.InstanceBufferView.InstanceCount, Cmd.SectionIndexStart, 0, 0);

    ID3D11Buffer* NullVertexBuffer = nullptr;
    uint32 NullStride = 0;
    uint32 NullOffset = 0;
    DeviceContext->IASetVertexBuffers(1, 1, &NullVertexBuffer, &NullStride, &NullOffset);

    return true;
}

bool FTranslucentRenderPass::DrawParticleSpriteCommand(const FRenderPassContext* Context, const FRenderCommand& Cmd)
{
    ID3D11DeviceContext* DeviceContext = Context->DeviceContext;
    const FParticleSpriteQuadResource QuadResource =
        FResourceManager::Get().GetOrCreateParticleSpriteQuadResource(Context->Device);
    if (!QuadResource.IsValid())
    {
        return false;
    }

    FShaderProgram* Program = GetParticleSpriteShaderProgram();
    if (Program == nullptr)
    {
        return false;
    }

    Program->Bind(DeviceContext);

    ID3D11DepthStencilState* DepthState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::DepthReadOnly);
    ID3D11RasterizerState* RasterizerState = FResourceManager::Get().GetOrCreateRasterizerState(
        Context->RenderBus->GetViewMode() == EViewMode::Wireframe ? ERasterizerType::WireFrame : ERasterizerType::SolidBackCull);
    ID3D11SamplerState* Sampler = FResourceManager::Get().GetOrCreateSamplerState(ESamplerType::EST_Linear);
    DeviceContext->OMSetDepthStencilState(DepthState, 0);
    DeviceContext->RSSetState(RasterizerState);
    DeviceContext->PSSetSamplers(0, 1, &Sampler);

    ID3D11ShaderResourceView* DefaultDiffuseSRV = FResourceManager::Get().GetDefaultWhiteSRV();
    DeviceContext->PSSetShaderResources(0, 1, &DefaultDiffuseSRV);
    if (Cmd.Material != nullptr)
    {
        Cmd.Material->BindParameters(DeviceContext, Program->PS);
    }

    ID3D11BlendState* BlendState = Cmd.Material != nullptr
        ? FResourceManager::Get().GetOrCreateBlendState(Cmd.Material->GetBlendStateDesc())
        : FResourceManager::Get().GetOrCreateBlendState(EBlendType::AlphaBlend);
    DeviceContext->OMSetBlendState(BlendState, nullptr, 0xFFFFFFFF);

    ID3D11Buffer* VertexBuffers[2] = { QuadResource.VertexBuffer, Cmd.InstanceBufferView.Buffer };
    uint32 Strides[2] = { QuadResource.VertexStride, Cmd.InstanceBufferView.Stride };
    uint32 Offsets[2] = { 0, Cmd.InstanceBufferView.Offset };
    DeviceContext->IASetVertexBuffers(0, 2, VertexBuffers, Strides, Offsets);
    DeviceContext->IASetIndexBuffer(QuadResource.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    DeviceContext->DrawIndexedInstanced(QuadResource.IndexCount, Cmd.InstanceBufferView.InstanceCount, 0, 0, 0);

    ID3D11Buffer* NullVertexBuffer = nullptr;
    uint32 NullStride = 0;
    uint32 NullOffset = 0;
    DeviceContext->IASetVertexBuffers(1, 1, &NullVertexBuffer, &NullStride, &NullOffset);

    return true;
}
