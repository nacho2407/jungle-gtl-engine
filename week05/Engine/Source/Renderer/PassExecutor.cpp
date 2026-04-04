#include "Renderer/PassExecutor.h"

#include <algorithm>

#include "Renderer/Material.h"
#include "Renderer/ObjectUniformStream.h"
#include "Renderer/RenderMesh.h"
#include "Renderer/Renderer.h"
#include "Renderer/SceneRenderer.h"

namespace
{
	constexpr ERenderPass GPassExecutionOrder[] =
	{
		ERenderPass::World,
		ERenderPass::NoDepth,
		ERenderPass::UI,
	};

	FRasterizerStateOption MakePassRasterizerState(ERenderPass RenderPass)
	{
		(void)RenderPass;

		FRasterizerStateOption State;
		State.FillMode = D3D11_FILL_SOLID;
		State.CullMode = D3D11_CULL_NONE;
		return State;
	}

	FDepthStencilStateOption MakePassDepthState(ERenderPass RenderPass)
	{
		FDepthStencilStateOption State;

		if (RenderPass == ERenderPass::World)
		{
			State.DepthEnable = true;
			State.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			State.DepthFunc = D3D11_COMPARISON_LESS;
			return State;
		}

		State.DepthEnable = false;
		State.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		return State;
	}

	FBlendStateOption MakePassBlendState(ERenderPass RenderPass)
	{
		(void)RenderPass;

		FBlendStateOption State;
		State.BlendEnable = true;
		State.SrcBlend = D3D11_BLEND_SRC_ALPHA;
		State.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		State.BlendOp = D3D11_BLEND_OP_ADD;
		State.SrcBlendAlpha = D3D11_BLEND_ONE;
		State.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		State.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		return State;
	}

	void BindPassState(FRenderer& Renderer, ERenderPass RenderPass)
	{
		FRenderStateManager* RenderStateManager = Renderer.GetRenderStateManager().get();
		if (!RenderStateManager)
		{
			return;
		}

		RenderStateManager->BindState(RenderStateManager->GetOrCreateRasterizerState(MakePassRasterizerState(RenderPass)));
		RenderStateManager->BindState(RenderStateManager->GetOrCreateDepthStencilState(MakePassDepthState(RenderPass)));
		RenderStateManager->BindState(RenderStateManager->GetOrCreateBlendState(MakePassBlendState(RenderPass)));
	}

	void ClearUnusedMaterialConstantBuffers(ID3D11DeviceContext* DeviceContext, uint32 PreviousCount, uint32 CurrentCount)
	{
		if (!DeviceContext || CurrentCount >= PreviousCount)
		{
			return;
		}

		ID3D11Buffer* NullBuffer = nullptr;
		for (uint32 Index = CurrentCount; Index < PreviousCount; ++Index)
		{
			const UINT Slot = FMaterial::MaterialCBStartSlot + static_cast<UINT>(Index);
			DeviceContext->VSSetConstantBuffers(Slot, 1, &NullBuffer);
			DeviceContext->PSSetConstantBuffers(Slot, 1, &NullBuffer);
		}
	}
}

void FPassExecutor::Execute(const FSceneRenderFrame& Packet) const
{
	if (!Renderer || !Renderer->DeviceContext || !Renderer->ObjectUniformStream)
	{
		return;
	}

	UpdateUploadedMeshes(Packet);
	Renderer->ObjectUniformStream->UploadFrame();
	Renderer->RenderStateManager->RebindState();

	Renderer->SetConstantBuffers();

	for (ERenderPass RenderPass : GPassExecutionOrder)
	{
		const TArray<FMeshDrawCommand>& PassCommands = Packet.GetPassQueue(RenderPass);
		BindPassState(*Renderer, RenderPass);
		ExecuteQueue(PassCommands);
	}

	if (!Packet.OutlineItems.empty())
	{
		Renderer->RenderOutlines(Packet.OutlineItems);
	}
}

void FPassExecutor::UpdateUploadedMeshes(const FSceneRenderFrame& Packet) const
{
	for (FRenderMesh* Mesh : Packet.MeshUploads)
	{
		if (Mesh)
		{
			Mesh->UpdateVertexAndIndexBuffer(Renderer->Device, Renderer->DeviceContext);
		}
	}
}

void FPassExecutor::ExecuteQueue(const TArray<FMeshDrawCommand>& InCommands) const
{
	if (!Renderer || !Renderer->DeviceContext || InCommands.empty())
	{
		return;
	}

	FMaterial* CurrentMaterial = nullptr;
	uint32 CurrentMaterialConstantBufferCount = 0;
	FRenderMesh* CurrentMesh = nullptr;
	D3D11_PRIMITIVE_TOPOLOGY CurrentTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

	for (const FMeshDrawCommand& Command : InCommands)
	{
		if (!Command.RenderMesh)
		{
			continue;
		}

		FMaterial* Material = Command.Material ? Command.Material : Renderer->GetDefaultMaterial();
		if (!Material)
		{
			continue;
		}

		if (Material != CurrentMaterial)
		{
			const uint32 PreviousMaterialConstantBufferCount = CurrentMaterialConstantBufferCount;
			Material->Bind(Renderer->DeviceContext);
			CurrentMaterialConstantBufferCount = static_cast<uint32>(Material->GetConstantBuffers().size());
			ClearUnusedMaterialConstantBuffers(
				Renderer->DeviceContext,
				PreviousMaterialConstantBufferCount,
				CurrentMaterialConstantBufferCount);
			CurrentMaterial = Material;
		}

		if (CurrentMesh != Command.RenderMesh)
		{
			Command.RenderMesh->Bind(Renderer->DeviceContext);
			CurrentMesh = Command.RenderMesh;
		}

		const D3D11_PRIMITIVE_TOPOLOGY DesiredTopology = static_cast<D3D11_PRIMITIVE_TOPOLOGY>(Command.RenderMesh->Topology);
		if (DesiredTopology != CurrentTopology)
		{
			Renderer->DeviceContext->IASetPrimitiveTopology(DesiredTopology);
			CurrentTopology = DesiredTopology;
		}

		Renderer->ObjectUniformStream->BindAllocation(Command.ObjectUniformAllocation);

		if (!Command.RenderMesh->Indices.empty())
		{
			const uint32 TotalIndexCount = static_cast<uint32>(Command.RenderMesh->Indices.size());
			if (Command.IndexStart >= TotalIndexCount)
			{
				continue;
			}

			const uint32 RemainingIndexCount = TotalIndexCount - Command.IndexStart;
			const uint32 DrawCount = Command.IndexCount > 0
				? (std::min)(Command.IndexCount, RemainingIndexCount)
				: RemainingIndexCount;
			if (DrawCount == 0)
			{
				continue;
			}

			++Renderer->FrameDrawCallCount;
			Renderer->DeviceContext->DrawIndexed(DrawCount, Command.IndexStart, 0);
		}
		else if (!Command.RenderMesh->Vertices.empty())
		{
			++Renderer->FrameDrawCallCount;
			Renderer->DeviceContext->Draw(static_cast<UINT>(Command.RenderMesh->Vertices.size()), 0);
		}
	}
}

