#include "DepthPrePass.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/Material.h"
#include "Render/Resource/ShaderPaths.h"
#include "Render/Resource/VertexFactoryTypes.h"
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
	const TArray<FRenderCommand>& OpaqueCmds = RenderBus->GetCommands(ERenderPass::Opaque);

	for (const auto& Cmd : OpaqueCmds)
	{
		if (Cmd.Type == ERenderCommandType::PostProcessOutline)
		{
			continue;
		}

		if (Cmd.MeshBuffer == nullptr || !Cmd.MeshBuffer->IsValid())
		{
			continue;
		}

		ID3D11Buffer* VertexBuffer = Cmd.MeshBuffer->GetVertexBuffer().GetBuffer();
		uint32 VertexCount = Cmd.MeshBuffer->GetVertexBuffer().GetVertexCount();
		uint32 Stride = Cmd.MeshBuffer->GetVertexBuffer().GetStride();
		if (VertexBuffer == nullptr || VertexCount == 0 || Stride == 0)
		{
			continue;
		}

		Context->RenderResources->PerObjectConstantBuffer.Update(Context->DeviceContext, &Cmd.PerObjectConstants, sizeof(FPerObjectConstants));
		ID3D11Buffer* CB1 = Context->RenderResources->PerObjectConstantBuffer.GetBuffer();
		Context->DeviceContext->VSSetConstantBuffers(1, 1, &CB1);

		uint32 Offset = 0;
		Context->DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);

		FShaderProgram* Program = GetDepthPrepassProgram(Cmd.VertexFactoryType);
		if (!Program)
		{
			continue;
		}

		Program->Bind(Context->DeviceContext);
		BindVertexFactoryResources(Context->DeviceContext, Cmd.VertexFactoryType, Cmd);
        CheckOverrideViewMode(Context);  

		ID3D11Buffer* IndexBuffer = Cmd.MeshBuffer->GetIndexBuffer().GetBuffer();
		if (IndexBuffer != nullptr)
		{
			Context->DeviceContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
			Context->DeviceContext->DrawIndexed(Cmd.SectionIndexCount, Cmd.SectionIndexStart, 0);
		}
		else
		{
			Context->DeviceContext->Draw(VertexCount, 0);
		}
	}

	return true;
}

bool FDepthPrePass::End(const FRenderPassContext* Context)
{
	Context->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	return true;
}
