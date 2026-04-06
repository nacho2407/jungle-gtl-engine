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
		ERenderPass::Opaque,
		ERenderPass::Alpha,
		ERenderPass::NoDepth,
		ERenderPass::UI,
	};

	void BindPassState(FRenderer& Renderer, const FRenderPassState& PassState)
	{
		FRenderStateManager* RenderStateManager = Renderer.GetRenderStateManager().get();
		if (!RenderStateManager)
		{
			return;
		}

		RenderStateManager->BindState(RenderStateManager->GetOrCreateRasterizerState(PassState.RasterizerState));
		RenderStateManager->BindState(RenderStateManager->GetOrCreateDepthStencilState(PassState.DepthStencilState));
		RenderStateManager->BindState(RenderStateManager->GetOrCreateBlendState(PassState.BlendState));
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

void FPassExecutor::Execute(const FSceneRenderFrame& Frame) const
{
	if (!Renderer || !Renderer->DeviceContext || !Renderer->ObjectUniformStream)
	{
		return;
	}

	UpdateUploadedMeshes(Frame);
	Renderer->ObjectUniformStream->UploadFrame();
	Renderer->RenderStateManager->RebindState();

	Renderer->SetConstantBuffers();

	for (ERenderPass RenderPass : GPassExecutionOrder)
	{
		const TArray<FMeshDrawCommand>& PassCommands = Frame.GetPassQueue(RenderPass);
		if (PassCommands.empty()) continue; // 빈 패스면 스킵
		BindPassState(*Renderer, Frame.GetPassState(RenderPass));
		ExecuteQueue(PassCommands);
	}

	if (!Frame.OutlineItems.empty())
	{
		Renderer->RenderOutlines(Frame.OutlineItems);
	}
}

void FPassExecutor::UpdateUploadedMeshes(const FSceneRenderFrame& Frame) const
{
	for (FRenderMesh* Mesh : Frame.MeshUploads)
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
	uint32 CurrentObjectUniformAllocation = UINT32_MAX;

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

		if (CurrentObjectUniformAllocation != Command.ObjectUniformAllocation)
		{
			Renderer->ObjectUniformStream->BindAllocation(Command.ObjectUniformAllocation);
			CurrentObjectUniformAllocation = Command.ObjectUniformAllocation;
		}

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

