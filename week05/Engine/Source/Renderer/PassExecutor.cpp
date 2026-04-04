#include "Renderer/PassExecutor.h"

#include <algorithm>
#include <unordered_set>

#include "Renderer/Material.h"
#include "Renderer/MaterialBindingCache.h"
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

	bool NeedsDepthClearForPass(const TArray<FMeshDrawCommand>& Commands)
	{
		for (const FMeshDrawCommand& Command : Commands)
		{
			if (!Command.bDisableDepthTest)
			{
				return true;
			}
		}

		return false;
	}
}

void FPassExecutor::Execute(const FSceneRenderFrame& Packet) const
{
	if (!Renderer || !Renderer->DeviceContext || !Renderer->ObjectUniformStream || !Renderer->MaterialBindingCache)
	{
		return;
	}

	UpdateUploadedMeshes(Packet);
	Renderer->ObjectUniformStream->UploadFrame();
	FlushDirtyMaterialConstantBuffers(Packet);
	Renderer->MaterialBindingCache->Reset();
	Renderer->RenderStateManager->RebindState();

	Renderer->SetConstantBuffers();

	for (ERenderPass RenderPass : GPassExecutionOrder)
	{
		const TArray<FMeshDrawCommand>& PassCommands = Packet.GetPassQueue(RenderPass);

		if (RenderPass == ERenderPass::NoDepth && NeedsDepthClearForPass(PassCommands))
		{
			Renderer->ClearDepthBuffer();
		}

		ExecuteQueue(PassCommands);
	}

	if (!Packet.OutlineItems.empty())
	{
		Renderer->RenderOutlines(Packet.OutlineItems);
	}
}

void FPassExecutor::FlushDirtyMaterialConstantBuffers(const FSceneRenderFrame& Packet) const
{
	if (!Renderer || !Renderer->DeviceContext)
	{
		return;
	}

	auto UploadPassQueue = [&](const TArray<FMeshDrawCommand>& Commands)
	{
		std::unordered_set<FMaterial*> VisitedMaterials;
		for (const FMeshDrawCommand& Command : Commands)
		{
			FMaterial* Material = Command.Material ? Command.Material : Renderer->GetDefaultMaterial();
			if (!Material || !Material->HasDirtyConstantBuffers())
			{
				continue;
			}
			if (!VisitedMaterials.insert(Material).second)
			{
				continue;
			}

			for (FMaterialConstantBuffer& CB : Material->GetConstantBuffers())
			{
				CB.Upload(Renderer->DeviceContext);
			}
		}
	};

	for (ERenderPass RenderPass : GPassExecutionOrder)
	{
		UploadPassQueue(Packet.GetPassQueue(RenderPass));
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

		Material->Bind(Renderer->DeviceContext, Renderer->MaterialBindingCache.get());
		Renderer->RenderStateManager->BindState(Command.RasterizerState);
		Renderer->RenderStateManager->BindState(Command.DepthStencilState);
		Renderer->RenderStateManager->BindState(Command.BlendState);

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

