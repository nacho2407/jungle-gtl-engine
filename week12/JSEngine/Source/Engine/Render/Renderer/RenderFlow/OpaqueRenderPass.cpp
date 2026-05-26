#include "OpaqueRenderPass.h"
#include "GeometryDrawPacket.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/Material.h"
#include "Render/Resource/ShaderHelper.h"
#include "Render/Resource/ShadowAtlasManager.h"
#include "Render/Resource/VertexFactoryTypes.h"
#include "Core/Logging/SkinningStats.h"
#include "Core/ResourceManager.h"
#include "Component/PostProcess/Light/LightComponent.h"

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
            UE_LOG_WARNING("[Render] ShaderType None material cannot be drawn by OpaqueRenderPass: %s", Cmd.Material->GetName().c_str());
            return nullptr;
        }

        // VertexFactory는 Mesh 타입에 맞는 VS를 고르고, Material은 표면용 PS만 제공합니다.
        // 여기서 두 정보를 합쳐 실제 Draw에 사용할 FShaderProgram을 만듭니다.
        const FVertexFactoryDesc& VertexFactoryDesc = FVertexFactoryRegistry::Get(Cmd.VertexFactoryType);
        const FString& PixelShaderPath = Cmd.Material->GetPixelShaderPath();
        const FString& PixelEntryPoint = Cmd.Material->GetPixelShaderEntryPoint();

        FShaderStageKey VSKey;
        VSKey.FilePath = VertexFactoryDesc.VertexShaderPath;
        VSKey.EntryPoint = VertexFactoryDesc.BasePassVSEntry;
        VSKey.Target = "vs_5_0";
        VSKey.PermutationKey = PermutationKey;

        FShaderStageKey PSKey;
        PSKey.FilePath = PixelShaderPath;
        PSKey.EntryPoint = PixelEntryPoint;
        PSKey.Target = "ps_5_0";
        PSKey.PermutationKey = PermutationKey;

        // PermutationKey는 ViewMode / LightCulling / Shadow / Material Feature를 한 번에 담습니다.
        // VS와 PS가 같은 define 조건으로 컴파일되어야 하므로 동일한 Macros를 넘깁니다.
        TArray<D3D_SHADER_MACRO> Macros = FShaderHelper::BuildUberLitMacros(PermutationKey);
        return FResourceManager::Get().GetOrCreateShaderProgram(
            VSKey,
            PSKey,
            Macros.data(),
            Macros.data(),
            &VertexFactoryDesc.VertexLayout);
    }

    void BindLightingResources(const FRenderPassContext* Context)
    {
        ID3D11SamplerState* ShadowSampler = FResourceManager::Get().GetOrCreateSamplerState(ESamplerType::EST_Shadow);
        Context->DeviceContext->PSSetSamplers(1, 1, &ShadowSampler);

        ID3D11ShaderResourceView* ShadowSRV = FShadowAtlasManager::Get().GetSRV();
        Context->DeviceContext->PSSetShaderResources(10, 1, &ShadowSRV);

        ID3D11ShaderResourceView* VSMSRV = FShadowAtlasManager::Get().GetVarianceSRV();
        Context->DeviceContext->PSSetShaderResources(11, 1, &VSMSRV);
        
        ID3D11ShaderResourceView* PointShadowCubeSRV = FShadowAtlasManager::Get().GetCubeSRV();
        Context->DeviceContext->PSSetShaderResources(12, 1, &PointShadowCubeSRV);

        ID3D11ShaderResourceView* ShadowInfoSRVs[] = {
            Context->RenderResources->LightShadowIndexBuffer.GetSRV(),
            Context->RenderResources->AtlasShadowBuffer.GetSRV(),
        };
        Context->DeviceContext->PSSetShaderResources(14, 2, ShadowInfoSRVs);
    }

    bool IsInstancedSurfaceCommand(const FRenderCommand& Cmd)
    {
        return SupportsInstancedSurfaceVertexFactory(Cmd.VertexFactoryType)
            && Cmd.HasInstanceBuffer();
    }

    FShaderProgram* GetParticleBeamShaderProgram()
    {
        const FVertexFactoryDesc& ParticleBeamDesc = FVertexFactoryRegistry::Get(EVertexFactoryType::ParticleBeam);

        FShaderStageKey VSKey;
        VSKey.FilePath = ParticleBeamDesc.VertexShaderPath;
        VSKey.EntryPoint = ParticleBeamDesc.BasePassVSEntry;
        VSKey.Target = "vs_5_0";

        FShaderStageKey PSKey;
        PSKey.FilePath = FShaderPaths::VFXParticleBeam;
        PSKey.EntryPoint = "PS";
        PSKey.Target = "ps_5_0";

        return FResourceManager::Get().GetOrCreateShaderProgram(
            VSKey,
            PSKey,
            nullptr,
            nullptr,
            &ParticleBeamDesc.VertexLayout);
    }

    bool IsParticleBeamCommand(const FRenderCommand& Cmd)
    {
        return Cmd.Type == ERenderCommandType::Particle
            && Cmd.VertexFactoryType == EVertexFactoryType::ParticleBeam
            && Cmd.HasInstanceBuffer();
    }

    bool BuildParticleBeamDrawResources(
        const FRenderPassContext* Context,
        const FRenderCommand& Cmd,
        FGeometryDrawPacket& OutDraw)
    {
        const FParticleSpriteQuadResource QuadResource =
            FResourceManager::Get().GetOrCreateParticleSpriteQuadResource(Context->Device);
        if (!QuadResource.IsValid())
        {
            UE_LOG_WARNING("[Particle] Beam draw skipped because the shared quad resource is missing.");
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

} // namespace

bool FOpaqueRenderPass::Initialize()
{
	return true;
}

bool FOpaqueRenderPass::Release()
{
    return true;
}

bool FOpaqueRenderPass::Begin(const FRenderPassContext* Context)
{

	if (IsDebugViewMode(Context->RenderBus->GetViewMode()))
	{
		OutSRV = PrevPassSRV;
		OutRTV = PrevPassRTV;
		return true;
	}

	const FRenderTargetSet* RenderTargets = Context->RenderTargets;
	ID3D11RenderTargetView* RTVs[1] = {
		RenderTargets->SceneColorRTV
	};
	ID3D11DepthStencilView* DSV = RenderTargets->DepthStencilView;

	// DepthPrePass is used as an input for earlier screen-space/light-culling work.
	// Opaque rendering must not depend on exact depth equality with that prepass,
	// otherwise runtime camera precision can leave horizontal holes in the GBuffer.
	Context->DeviceContext->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	Context->DeviceContext->OMSetRenderTargets(ARRAYSIZE(RTVs), RTVs, DSV);
	OutSRV = RenderTargets->SceneColorSRV;
	OutRTV = RenderTargets->SceneColorRTV;

	Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    BindLightingResources(Context);

	return true;
}

bool FOpaqueRenderPass::DrawCommand(const FRenderPassContext* Context)  
{  
   const FRenderBus* RenderBus = Context->RenderBus;  
   const TArray<FRenderCommand>& Commands = RenderBus->GetCommands(ERenderPass::Opaque);  
   if (IsDebugViewMode(RenderBus->GetViewMode()))
	   return true;

   if (Commands.empty())  
	   return true;  

   const EViewMode ViewMode = RenderBus->GetViewMode();

   for (const FRenderCommand& Cmd : Commands)  
   {  
	   if (!DrawEachCommand(Context, Cmd, ViewMode)) 
	       continue;        // Draw 실패 시 동작 추가할 거면 여기에 추가
   }  

   return true;  
}

bool FOpaqueRenderPass::DrawEachCommand(const FRenderPassContext* Context, const FRenderCommand& Cmd, const EViewMode ViewMode)
{
    ID3D11DeviceContext* DeviceContext = Context->DeviceContext;
    
    Context->RenderResources->PerObjectConstantBuffer.Update(DeviceContext, &Cmd.PerObjectConstants, sizeof(FPerObjectConstants));  
    ID3D11Buffer* cb1 = Context->RenderResources->PerObjectConstantBuffer.GetBuffer();  
    DeviceContext->VSSetConstantBuffers(1, 1, &cb1);  
    DeviceContext->PSSetConstantBuffers(1, 1, &cb1);

    if (Cmd.Type == ERenderCommandType::PostProcessOutline)  
    {  
        return false;  
    }  

    // TODO: Particle 렌더링도 기존 구조에 좀 더 융화시킬 수 없는지 고민해보기
    if (IsParticleBeamCommand(Cmd))
    {
        FShaderProgram* Program = GetParticleBeamShaderProgram();
        if (Program == nullptr)
        {
            UE_LOG_WARNING("[Particle] Beam draw skipped because ParticleBeam shader failed to compile.");
            return false;
        }

        FGeometryDrawPacket DrawResources;
        if (!BuildParticleBeamDrawResources(Context, Cmd, DrawResources))
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
            ID3D11DepthStencilState* DepthState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::Default);
            ID3D11BlendState* BlendState = FResourceManager::Get().GetOrCreateBlendState(EBlendType::Opaque);
            ID3D11RasterizerState* RasterizerState = FResourceManager::Get().GetOrCreateRasterizerState(ERasterizerType::SolidBackCull);
            ID3D11SamplerState* Sampler = FResourceManager::Get().GetOrCreateSamplerState(ESamplerType::EST_Linear);
            DeviceContext->OMSetDepthStencilState(DepthState, 0);
            DeviceContext->OMSetBlendState(BlendState, nullptr, 0xFFFFFFFF);
            DeviceContext->RSSetState(RasterizerState);
            DeviceContext->PSSetSamplers(0, 1, &Sampler);
        }
        ID3D11RasterizerState* ParticleRasterizerState = FResourceManager::Get().GetOrCreateRasterizerState(ERasterizerType::SolidNoCull);
        DeviceContext->RSSetState(ParticleRasterizerState);

        ID3D11ShaderResourceView* DefaultDiffuseSRV = FResourceManager::Get().GetDefaultWhiteSRV();
        DeviceContext->PSSetShaderResources(0, 1, &DefaultDiffuseSRV);
        if (Cmd.Material != nullptr)
        {
            Cmd.Material->BindParameters(DeviceContext, Program->PS);
        }
        return ExecuteGeometryDrawPacket(DeviceContext, DrawResources);
    }

    const bool bInstancedSurfaceDraw = IsInstancedSurfaceCommand(Cmd);
    FGeometryDrawPacket DrawResources;
    if (!BuildMeshGeometryDrawPacket(Cmd, DrawResources, bInstancedSurfaceDraw))
    {
        return false;
    }

    if (Cmd.Material)
    {
        uint32 PermutationKey = 0;
        PermutationKey |= GetLightingModelPermutationKey(ViewMode);
        PermutationKey |= GetLightCullingPermutationKey(Context);
        PermutationKey |= GetShadowMapPermutationKey(Context, true);
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

    auto DSState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::Default);
    DeviceContext->OMSetDepthStencilState(DSState, 0);

    CheckOverrideViewMode(Context);  

    const bool bGPUSkinnedDraw =
        Cmd.Type == ERenderCommandType::SkeletalMesh && Cmd.bUseBoneMatrixConstants;
    if (bGPUSkinnedDraw)
    {
        FSkinningStats::Get().AddGPUSkinnedDraw(Cmd.SkinningWorkVertexCount, Cmd.AvgBoneInfluencePerVertex);
    }
    
    return ExecuteGeometryDrawPacket(DeviceContext, DrawResources);
}

bool FOpaqueRenderPass::End(const FRenderPassContext* Context)
{
    //ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr, nullptr };
    //Context->DeviceContext->VSSetShaderResources(4, 3, nullSRVs);
    //Context->DeviceContext->PSSetShaderResources(4, 3, nullSRVs);
    ID3D11ShaderResourceView* nullSRV = nullptr;
    Context->DeviceContext->PSSetShaderResources(16, 1, &nullSRV);
    return true;
}
