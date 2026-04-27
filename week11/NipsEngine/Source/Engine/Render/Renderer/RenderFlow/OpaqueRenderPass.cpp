#include "OpaqueRenderPass.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/Material.h"
#include "Render/Resource/ShaderHelper.h"
#include "Render/Resource/ShadowAtlasManager.h"
#include "Core/ResourceManager.h"
#include "Component/PostProcess/Light/LightComponent.h"

bool FOpaqueRenderPass::Initialize()
{
    return true;
}

bool FOpaqueRenderPass::Begin(const FRenderPassContext* Context)
{




    const FRenderTargetSet* RenderTargets = Context->RenderTargets;
    ID3D11RenderTargetView* RTVs[3] = { 
		RenderTargets->SceneColorRTV, 
		RenderTargets->SceneNormalRTV,
		RenderTargets->SceneWorldPosRTV
	};
    ID3D11DepthStencilView* DSV = RenderTargets->DepthStencilView;

	Context->DeviceContext->OMSetRenderTargets(ARRAYSIZE(RTVs), RTVs, DSV);
    OutSRV = RenderTargets->SceneColorSRV;
    OutRTV = RenderTargets->SceneColorRTV;

	Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D11SamplerState* ShadowSampler = FResourceManager::Get().GetOrCreateSamplerState(ESamplerType::EST_Shadow);
	Context->DeviceContext->PSSetSamplers(1, 1, &ShadowSampler);

	///// debugging
	ID3D11ShaderResourceView* VSMSRV = FShadowAtlasManager::Get().VarianceShadowSRV.Get();
    Context->DeviceContext->PSSetShaderResources(11, 1, &VSMSRV);
    ///// debugging



	ID3D11ShaderResourceView* ShadowInfoSRVs[] = {
		Context->RenderResources->LightShadowIndexBuffer.GetSRV(),
		Context->RenderResources->AtlasShadowBuffer.GetSRV(),
	};
	Context->DeviceContext->PSSetShaderResources(14, 2, ShadowInfoSRVs);

    return true;
}

bool FOpaqueRenderPass::DrawCommand(const FRenderPassContext* Context)  
{  
   const FRenderBus* RenderBus = Context->RenderBus;  
   const TArray<FRenderCommand>& Commands = RenderBus->GetCommands(ERenderPass::Opaque);  

   if (Commands.empty())  
       return true;  

   for (const FRenderCommand& Cmd : Commands)  
   {  
       Context->RenderResources->PerObjectConstantBuffer.Update(Context->DeviceContext, &Cmd.PerObjectConstants, sizeof(FPerObjectConstants));  
       ID3D11Buffer* cb1 = Context->RenderResources->PerObjectConstantBuffer.GetBuffer();  
       Context->DeviceContext->VSSetConstantBuffers(1, 1, &cb1);  
       Context->DeviceContext->PSSetConstantBuffers(1, 1, &cb1);

	   ID3D11ShaderResourceView *ShadowSRV = FShadowAtlasManager::Get().GetSRV();
	   Context->DeviceContext->PSSetShaderResources(10, 1, &ShadowSRV);

       if (Cmd.Type == ERenderCommandType::PostProcessOutline)  
       {  
           continue;  
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

	   uint32 PermutationKey = (uint32)ELightingModel::Unlit;
	   switch (Context->RenderBus->GetViewMode())
	   {
	   case EViewMode::Lit_Gouraud:
		   PermutationKey = (uint32)ELightingModel::Gouraud;
		   break;
	   case EViewMode::Lit_Lambert:
		   PermutationKey = (uint32)ELightingModel::Lambert;
		   break;
	   case EViewMode::Lit_BlinnPhong:
		   PermutationKey = (uint32)ELightingModel::BlinnPhong;
		   break;
	   case EViewMode::Heatmap:
		   PermutationKey = (uint32)ELightingModel::Heatmap;
		   break;
	   }

       if (RenderBus->GetLightCullMode() == ELightCullMode::Clustered)
           PermutationKey |= (uint32)EShaderFeature::ClusterCull;
       else if (RenderBus->GetLightCullMode() == ELightCullMode::Tiled)
           PermutationKey |= (uint32)EShaderFeature::TileCull;

	   // ShadowMap Permutation Key 조합
       for (const FShadowLightRequest& Request : RenderBus->ShadowLightRequests)
       {
           if (!Request.bCastShadows || !Request.LightComponent) continue;
           ULightComponent* LightComp = Cast<ULightComponent>(Request.LightComponent);
           if (!LightComp) continue;
           switch (LightComp->GetShadowMapType())
           {
		   case EShadowMap::CSM:
		   {
			   PermutationKey |= (uint32)EShaderFeature::ShadowCSM;
			   break;
		   }

		   case EShadowMap::PSM:
		   {
			   PermutationKey |= (uint32)EShaderFeature::ShadowPSM;
			   break;
		   }
           default: break;
           }
           break;
       }

       if (Cmd.Material)
       {
		   if (Cmd.Material->HasDiffuseMap()) PermutationKey |= (uint32)EShaderFeature::HasDiffuseMap;
		   if (Cmd.Material->HasNormalMap()) PermutationKey |= (uint32)EShaderFeature::HasNormalMap;
		   if (Cmd.Material->HasSpecularMap()) PermutationKey |= (uint32)EShaderFeature::HasSpecularMap;
		   if (Cmd.Material->HasEmissiveMap()) PermutationKey |= (uint32)EShaderFeature::HasEmissiveMap;
		   if (Cmd.Material->HasAlphaMask()) PermutationKey |= (uint32)EShaderFeature::HasAlphaMask;

           Cmd.Material->Bind(Context->DeviceContext, PermutationKey);
       }

       auto DSState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::DepthReadOnly);
       Context->DeviceContext->OMSetDepthStencilState(DSState, 0);

       CheckOverrideViewMode(Context);  

       Context->DeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);  

       ID3D11Buffer* indexBuffer = Cmd.MeshBuffer->GetIndexBuffer().GetBuffer();  
       if (indexBuffer != nullptr)  
       {  
           uint32 indexStart = Cmd.SectionIndexStart;  
           uint32 indexCount = Cmd.SectionIndexCount;  
           Context->DeviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);  
           Context->DeviceContext->DrawIndexed(indexCount, indexStart, 0);  
       }  
       else  
       {  
           Context->DeviceContext->Draw(vertexCount, 0);  
       }  
   }  

   return true;  
}

bool FOpaqueRenderPass::End(const FRenderPassContext* Context)
{
	//ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr, nullptr };
	//Context->DeviceContext->VSSetShaderResources(4, 3, nullSRVs);
	//Context->DeviceContext->PSSetShaderResources(4, 3, nullSRVs);
    return true;
}

bool FOpaqueRenderPass::Release()
{
    return true;
}
