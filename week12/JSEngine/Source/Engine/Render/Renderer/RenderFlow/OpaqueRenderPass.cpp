#include "OpaqueRenderPass.h"
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
    
    DeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);  

    const bool bGPUSkinnedDraw =
        Cmd.Type == ERenderCommandType::SkeletalMesh && Cmd.bUseBoneMatrixConstants;
    if (bGPUSkinnedDraw)
    {
        FSkinningStats::Get().AddGPUSkinnedDraw(Cmd.SkinningWorkVertexCount, Cmd.AvgBoneInfluencePerVertex);
    }
    
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

bool FOpaqueRenderPass::End(const FRenderPassContext* Context)
{
    //ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr, nullptr };
    //Context->DeviceContext->VSSetShaderResources(4, 3, nullSRVs);
    //Context->DeviceContext->PSSetShaderResources(4, 3, nullSRVs);
    ID3D11ShaderResourceView* nullSRV = nullptr;
    Context->DeviceContext->PSSetShaderResources(16, 1, &nullSRV);
    return true;
}