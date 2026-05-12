#include "OpaqueRenderPass.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/Material.h"
#include "Render/Resource/ShaderHelper.h"
#include "Render/Resource/ShadowAtlasManager.h"
#include "Render/Resource/VertexFactoryTypes.h"
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

    // DepthPrePass is used as an input for earlier screen-space/light-culling work.
    // Opaque rendering must not depend on exact depth equality with that prepass,
    // otherwise runtime camera precision can leave horizontal holes in the GBuffer.
    Context->DeviceContext->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	Context->DeviceContext->OMSetRenderTargets(ARRAYSIZE(RTVs), RTVs, DSV);
    OutSRV = RenderTargets->SceneColorSRV;
    OutRTV = RenderTargets->SceneColorRTV;

	Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D11SamplerState* ShadowSampler = FResourceManager::Get().GetOrCreateSamplerState(ESamplerType::EST_Shadow);
	Context->DeviceContext->PSSetSamplers(1, 1, &ShadowSampler);

	///// debugging
	/*ID3D11ShaderResourceView* VSMSRV = FShadowAtlasManager::Get().VarianceShadowSRV.Get();
    Context->DeviceContext->PSSetShaderResources(11, 1, &VSMSRV);
    *////// debugging
    ID3D11ShaderResourceView* VSMSRV = FShadowAtlasManager::Get().GetVarianceSRV();
    Context->DeviceContext->PSSetShaderResources(11, 1, &VSMSRV);



	ID3D11ShaderResourceView* ShadowInfoSRVs[] = {
		Context->RenderResources->LightShadowIndexBuffer.GetSRV(),
		Context->RenderResources->AtlasShadowBuffer.GetSRV(),
	};
	Context->DeviceContext->PSSetShaderResources(14, 2, ShadowInfoSRVs);

	//ID3D11ShaderResourceView* VSMSRV = FShadowAtlasManager::Get().VarianceShadowSRV.Get();
 //   Context->DeviceContext->PSSetShaderResources(16, 1, &VSMSRV);

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
       ID3D11ShaderResourceView* PointShadowCubeSRV = FShadowAtlasManager::Get().GetCubeSRV();
       Context->DeviceContext->PSSetShaderResources(12, 1, &PointShadowCubeSRV);

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
       bool bShadowApplied = false; // 추가

	   // ShadowMap Permutation Key 조합
       for (const FShadowLightRequest& Request : RenderBus->ShadowLightRequests)
       {
           if (!Request.bCastShadows || !Request.LightComponent) continue;
           if (Request.Type != EShadowLightType::SLT_Directional) continue;

           ULightComponent* LightComp = Cast<ULightComponent>(Request.LightComponent);
           if (!LightComp) continue;
           switch (LightComp->GetShadowMapType())
           {
		   case EShadowMap::CSM:
		   {
			   PermutationKey |= (uint32)EShaderFeature::ShadowCSM;
			   if (RenderBus->GetCascadeVis())
				   PermutationKey |= (uint32)EShaderFeature::CascadeVis;
               bShadowApplied = true;
			   break;
		   }

		   case EShadowMap::PSM:
		   {
			   PermutationKey |= (uint32)EShaderFeature::ShadowPSM;
               bShadowApplied = true;

			   break;
		   }
           default: break;
           }
           break;
       }
       // VSM 모드 + 그림자 활성일 때만 OR
       if (bShadowApplied and Context->RenderBus->GetShadowFilterMode() == EShadowFilter::VSM)
       {
           PermutationKey |= (uint32)EShaderFeature::ShadowVSM;
       }
       if (Cmd.Material)
       {
		   if (Cmd.Material->HasDiffuseMap()) PermutationKey |= (uint32)EShaderFeature::HasDiffuseMap;
		   if (Cmd.Material->HasNormalMap()) PermutationKey |= (uint32)EShaderFeature::HasNormalMap;
		   if (Cmd.Material->HasSpecularMap()) PermutationKey |= (uint32)EShaderFeature::HasSpecularMap;
		   if (Cmd.Material->HasEmissiveMap()) PermutationKey |= (uint32)EShaderFeature::HasEmissiveMap;
		   if (Cmd.Material->HasAlphaMask()) PermutationKey |= (uint32)EShaderFeature::HasAlphaMask;

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
           FShaderProgram* Program = FResourceManager::Get().GetOrCreateShaderProgram(
               VSKey,
               PSKey,
               Macros.data(),
               Macros.data());

           if (!Program)
           {
               return false;
           }

           Program->Bind(Context->DeviceContext);
           Cmd.Material->BindRenderStates(Context->DeviceContext);
           Cmd.Material->BindParameters(Context->DeviceContext, Program->PS);

           // 현재는 CPU Skinning이라 추가 바인딩이 없지만, GPU Skinning에서는 여기서 Bone Buffer가 붙습니다.
           BindVertexFactoryResources(Context->DeviceContext, Cmd.VertexFactoryType, Cmd);
       }

       auto DSState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::Default);
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
    ID3D11ShaderResourceView* nullSRV = nullptr;
    Context->DeviceContext->PSSetShaderResources(16, 1, &nullSRV);
    return true;
}

bool FOpaqueRenderPass::Release()
{
    return true;
}
