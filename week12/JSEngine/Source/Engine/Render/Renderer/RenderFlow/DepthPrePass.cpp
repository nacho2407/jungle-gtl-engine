#include "DepthPrePass.h"
#include "GeometryDrawPacket.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/Material.h"
#include "Render/Resource/ShaderPaths.h"
#include "Render/Resource/VertexFactoryTypes.h"
#include "Core/Logging/SkinningStats.h"
#include "Core/ResourceManager.h"

namespace
{
	FShaderProgram* GetDepthPrepassProgram(EVertexFactoryType VertexFactoryType)
	{
		const FVertexFactoryDesc& VertexFactory = FVertexFactoryRegistry::Get(VertexFactoryType);

		FShaderStageKey VSKey;
		VSKey.FilePath = VertexFactory.DepthPassVSPath.empty() ? FShaderPaths::DepthPrepass : VertexFactory.DepthPassVSPath;
		VSKey.EntryPoint = VertexFactory.DepthPassVSEntry;

		FShaderStageKey PSKey;
		PSKey.FilePath = FShaderPaths::DepthPrepass;
		PSKey.EntryPoint = "DepthPrepassPS";

		return FResourceManager::Get().GetOrCreateShaderProgram(
			VSKey,
			PSKey,
			nullptr,
			nullptr,
			&VertexFactory.PositionOnlyLayout);
	}
}

bool FDepthPrePass::Initialize()
{
	return true;
}

bool FDepthPrePass::Release()
{
	return true;
}

bool FDepthPrePass::Begin(const FRenderPassContext* Context)
{
	const FRenderTargetSet* RenderTargets = Context->RenderTargets;
	ID3D11DepthStencilView* DSV = RenderTargets->DepthStencilView;
	Context->DeviceContext->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	Context->DeviceContext->OMSetRenderTargets(0, nullptr, DSV);
	OutSRV = RenderTargets->SceneDepthSRV;
	OutRTV = nullptr;

	ID3D11DepthStencilState* DepthState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::Default);
	Context->DeviceContext->OMSetDepthStencilState(DepthState, 0);

	Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	return true;
}

bool FDepthPrePass::DrawCommand(const FRenderPassContext* Context)
{
	const FRenderBus* RenderBus = Context->RenderBus;
	const TArray<FRenderCommand>& DepthPrepassCmds = RenderBus->GetDepthPrepassCommands();

	for (const auto& Cmd : DepthPrepassCmds)
	{
		FGeometryDrawPacket DrawPacket;
		if (!BuildMeshGeometryDrawPacket(Cmd, DrawPacket))
		{
			continue;
		}

		Context->RenderResources->PerObjectConstantBuffer.Update(Context->DeviceContext, &Cmd.PerObjectConstants, sizeof(FPerObjectConstants));
		ID3D11Buffer* CB1 = Context->RenderResources->PerObjectConstantBuffer.GetBuffer();
		Context->DeviceContext->VSSetConstantBuffers(1, 1, &CB1);

		FShaderProgram* Program = GetDepthPrepassProgram(Cmd.VertexFactoryType);
		if (!Program)
		{
			continue;
		}

		Program->Bind(Context->DeviceContext);
		BindVertexFactoryResources(
			Context->DeviceContext,
			Cmd.VertexFactoryType,
			Context->RenderBus->GetBoneMatrixConstants(Cmd),
			Context->RenderResources,
			Cmd.BoneMatrixConstantBuffer);
        CheckOverrideViewMode(Context);  

		const bool bGPUSkinnedDraw =
			Cmd.Type == ERenderCommandType::SkeletalMesh && Cmd.bUseBoneMatrixConstants;
		if (bGPUSkinnedDraw)
		{
			FSkinningStats::Get().AddGPUSkinnedDraw(
				Cmd.SkinningWorkVertexCount,
				Cmd.AvgBoneInfluencePerVertex);
		}
		ExecuteGeometryDrawPacket(Context->DeviceContext, DrawPacket);
	}

	return true;
}

bool FDepthPrePass::End(const FRenderPassContext* Context)
{
	Context->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	return true;
}
