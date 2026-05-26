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
            && Cmd.HasInstanceBuffer();
    }

    bool IsInstancedSurfaceCommand(const FRenderCommand& Cmd)
    {
        return SupportsInstancedSurfaceVertexFactory(Cmd.VertexFactoryType)
            && Cmd.HasInstanceBuffer();
    }

    struct FTranslucentDrawResources
    {
        ID3D11Buffer* VertexBuffers[2] = { nullptr, nullptr };
        uint32 Strides[2] = { 0, 0 };
        uint32 Offsets[2] = { 0, 0 };
        uint32 VertexBufferCount = 0;
        ID3D11Buffer* IndexBuffer = nullptr;
        uint32 IndexCount = 0;
        uint32 IndexStart = 0;
        uint32 VertexCount = 0;
        uint32 InstanceCount = 0;
        bool bInstanced = false;
    };

    void BindDefaultTranslucentStates(const FRenderPassContext* Context)
    {
        ID3D11DeviceContext* DeviceContext = Context->DeviceContext;
        ID3D11DepthStencilState* DepthState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::DepthReadOnly);
        ID3D11BlendState* BlendState = FResourceManager::Get().GetOrCreateBlendState(EBlendType::AlphaBlend);
        ID3D11RasterizerState* RasterizerState = FResourceManager::Get().GetOrCreateRasterizerState(
            Context->RenderBus->GetViewMode() == EViewMode::Wireframe ? ERasterizerType::WireFrame : ERasterizerType::SolidBackCull);
        ID3D11SamplerState* Sampler = FResourceManager::Get().GetOrCreateSamplerState(ESamplerType::EST_Linear);
        DeviceContext->OMSetDepthStencilState(DepthState, 0);
        DeviceContext->OMSetBlendState(BlendState, nullptr, 0xFFFFFFFF);
        DeviceContext->RSSetState(RasterizerState);
        DeviceContext->PSSetSamplers(0, 1, &Sampler);
    }

    bool BuildParticleSpriteDrawResources(
        const FRenderPassContext* Context,
        const FRenderCommand& Cmd,
        FTranslucentDrawResources& OutDraw)
    {
        const FParticleSpriteQuadResource QuadResource =
            FResourceManager::Get().GetOrCreateParticleSpriteQuadResource(Context->Device);
        if (!QuadResource.IsValid())
        {
            return false;
        }

        OutDraw.VertexBuffers[0] = QuadResource.VertexBuffer;
        OutDraw.VertexBuffers[1] = Cmd.InstanceBufferView.Buffer;
        OutDraw.Strides[0] = QuadResource.VertexStride;
        OutDraw.Strides[1] = Cmd.InstanceBufferView.Stride;
        OutDraw.Offsets[0] = 0;
        OutDraw.Offsets[1] = Cmd.InstanceBufferView.Offset;
        OutDraw.VertexBufferCount = 2;
        OutDraw.IndexBuffer = QuadResource.IndexBuffer;
        OutDraw.IndexCount = QuadResource.IndexCount;
        OutDraw.InstanceCount = Cmd.InstanceBufferView.InstanceCount;
        OutDraw.bInstanced = true;
        return true;
    }

    bool BuildMeshDrawResources(
        const FRenderCommand& Cmd,
        bool bInstancedSurfaceDraw,
        FTranslucentDrawResources& OutDraw)
    {
        if (Cmd.MeshBuffer == nullptr || !Cmd.MeshBuffer->IsValid())
        {
            return false;
        }

        ID3D11Buffer* VertexBuffer = Cmd.MeshBuffer->GetVertexBuffer().GetBuffer();
        const uint32 VertexCount = Cmd.MeshBuffer->GetVertexBuffer().GetVertexCount();
        const uint32 Stride = Cmd.MeshBuffer->GetVertexBuffer().GetStride();
        if (VertexBuffer == nullptr || VertexCount == 0 || Stride == 0)
        {
            return false;
        }

        ID3D11Buffer* IndexBuffer = Cmd.MeshBuffer->GetIndexBuffer().GetBuffer();
        if (bInstancedSurfaceDraw && (IndexBuffer == nullptr || Cmd.SectionIndexCount == 0))
        {
            return false;
        }

        OutDraw.VertexBuffers[0] = VertexBuffer;
        OutDraw.Strides[0] = Stride;
        OutDraw.Offsets[0] = 0;
        OutDraw.VertexBufferCount = 1;
        OutDraw.IndexBuffer = IndexBuffer;
        OutDraw.IndexCount = Cmd.SectionIndexCount;
        OutDraw.IndexStart = Cmd.SectionIndexStart;
        OutDraw.VertexCount = VertexCount;

        if (bInstancedSurfaceDraw)
        {
            OutDraw.VertexBuffers[1] = Cmd.InstanceBufferView.Buffer;
            OutDraw.Strides[1] = Cmd.InstanceBufferView.Stride;
            OutDraw.Offsets[1] = Cmd.InstanceBufferView.Offset;
            OutDraw.VertexBufferCount = 2;
            OutDraw.InstanceCount = Cmd.InstanceBufferView.InstanceCount;
            OutDraw.bInstanced = true;
        }
        return true;
    }

    bool ExecuteTranslucentDraw(ID3D11DeviceContext* DeviceContext, const FTranslucentDrawResources& Draw)
    {
        if (Draw.VertexBufferCount == 0 || Draw.VertexBuffers[0] == nullptr)
        {
            return false;
        }

        DeviceContext->IASetVertexBuffers(0, Draw.VertexBufferCount, Draw.VertexBuffers, Draw.Strides, Draw.Offsets);

        if (Draw.IndexBuffer != nullptr)
        {
            DeviceContext->IASetIndexBuffer(Draw.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
        }

        if (Draw.bInstanced)
        {
            if (Draw.IndexBuffer == nullptr || Draw.IndexCount == 0 || Draw.InstanceCount == 0)
            {
                return false;
            }
            DeviceContext->DrawIndexedInstanced(Draw.IndexCount, Draw.InstanceCount, Draw.IndexStart, 0, 0);
        }
        else if (Draw.IndexBuffer != nullptr)
        {
            DeviceContext->DrawIndexed(Draw.IndexCount, Draw.IndexStart, 0);
        }
        else
        {
            DeviceContext->Draw(Draw.VertexCount, 0);
        }

        if (Draw.VertexBufferCount > 1)
        {
            ID3D11Buffer* NullVertexBuffer = nullptr;
            uint32 NullStride = 0;
            uint32 NullOffset = 0;
            DeviceContext->IASetVertexBuffers(1, 1, &NullVertexBuffer, &NullStride, &NullOffset);
        }
        return true;
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
        FShaderProgram* Program = GetParticleSpriteShaderProgram();
        if (Program == nullptr)
        {
            return false;
        }

        Program->Bind(DeviceContext);

        if (Cmd.Material != nullptr)
        {
            Cmd.Material->BindRenderStates(DeviceContext);
        }
        else
        {
            BindDefaultTranslucentStates(Context);
        }

        ID3D11ShaderResourceView* DefaultDiffuseSRV = FResourceManager::Get().GetDefaultWhiteSRV();
        DeviceContext->PSSetShaderResources(0, 1, &DefaultDiffuseSRV);
        if (Cmd.Material != nullptr)
        {
            Cmd.Material->BindParameters(DeviceContext, Program->PS);
        }

        FTranslucentDrawResources DrawResources;
        return BuildParticleSpriteDrawResources(Context, Cmd, DrawResources)
            && ExecuteTranslucentDraw(DeviceContext, DrawResources);
    }

    const bool bInstancedSurfaceDraw = IsInstancedSurfaceCommand(Cmd);
    FTranslucentDrawResources DrawResources;
    if (!BuildMeshDrawResources(Cmd, bInstancedSurfaceDraw, DrawResources))
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

    return ExecuteTranslucentDraw(DeviceContext, DrawResources);
}
