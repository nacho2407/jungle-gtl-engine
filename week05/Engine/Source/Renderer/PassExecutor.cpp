#include "Renderer/PassExecutor.h"

#include <algorithm>
#include <unordered_set>

#include "Renderer/Material.h"
#include "Renderer/MaterialBindingCache.h"
#include "Renderer/ObjectUniformStream.h"
#include "Renderer/RenderMesh.h"
#include "Renderer/Renderer.h"
#include "Renderer/SceneRenderer.h"

void FPassExecutor::Execute(const FSceneFramePacket& Packet) const
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

	const TArray<FMeshDrawCommand>& BaseCommands = Packet.PassCommandQueues.BasePassCommands;
	const TArray<FMeshDrawCommand>& OverlayCommands = Packet.PassCommandQueues.OverlayPassCommands;
	const TArray<FMeshDrawCommand>& UICommands = Packet.PassCommandQueues.UIPassCommands;
	const TArray<FMeshDrawCommand>& OutlineMaskCommands = Packet.PassCommandQueues.OutlineMaskCommands;
	const TArray<FMeshDrawCommand>& OutlineCompositeCommands = Packet.PassCommandQueues.OutlineCompositeCommands;

	ExecuteQueue(BaseCommands);

	bool bNeedsDepthClearForOverlay = false;
	for (const FMeshDrawCommand& OverlayCommand : OverlayCommands)
	{
		if (!OverlayCommand.bDisableDepthTest)
		{
			bNeedsDepthClearForOverlay = true;
			break;
		}
	}

	if (bNeedsDepthClearForOverlay)
	{
		Renderer->ClearDepthBuffer();
	}

	ExecuteQueue(OverlayCommands);
	ExecuteQueue(UICommands);
	ExecuteQueue(OutlineMaskCommands);
	ExecuteQueue(OutlineCompositeCommands);

	if (!Packet.OutlineItems.empty())
	{
		Renderer->RenderOutlines(Packet.OutlineItems);
	}
}

void FPassExecutor::ExecutePass(const FSceneFramePacket& Packet, EMeshPass MeshPass) const
{
	ExecuteQueue(Packet.PassCommandQueues.GetQueue(MeshPass));

	if (Renderer && MeshPass == EMeshPass::OutlineComposite && !Packet.OutlineItems.empty())
	{
		Renderer->RenderOutlines(Packet.OutlineItems);
	}
}

void FPassExecutor::FlushDirtyMaterialConstantBuffers(const FSceneFramePacket& Packet) const
{
	if (!Renderer || !Renderer->DeviceContext)
	{
		return;
	}

	std::unordered_set<FMaterial*> Visited;

	auto UploadPassQueue = [&](const TArray<FMeshDrawCommand>& Commands)
	{
		for (const FMeshDrawCommand& Command : Commands)
		{
			FMaterial* Material = Command.Material ? Command.Material : Renderer->GetDefaultMaterial();
			if (!Material)
			{
				continue;
			}

			if (!Visited.insert(Material).second)
			{
				continue;
			}

			for (FMaterialConstantBuffer& CB : Material->GetConstantBuffers())
			{
				CB.Upload(Renderer->DeviceContext);
			}
		}
	};

	UploadPassQueue(Packet.PassCommandQueues.BasePassCommands);
	UploadPassQueue(Packet.PassCommandQueues.OverlayPassCommands);
	UploadPassQueue(Packet.PassCommandQueues.UIPassCommands);
	UploadPassQueue(Packet.PassCommandQueues.OutlineMaskCommands);
	UploadPassQueue(Packet.PassCommandQueues.OutlineCompositeCommands);
}

void FPassExecutor::UpdateUploadedMeshes(const FSceneFramePacket& Packet) const
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

		if (Command.bDisableCulling)
		{
			FRasterizerStateOption RasterizerOption = Material->GetRasterizerOption();
			RasterizerOption.CullMode = D3D11_CULL_NONE;
			Renderer->RenderStateManager->BindState(Renderer->RenderStateManager->GetOrCreateRasterizerState(RasterizerOption));
		}
		else
		{
			Renderer->RenderStateManager->BindState(Material->GetRasterizerState());
		}

		if (Command.bDisableDepthTest || Command.bDisableDepthWrite)
		{
			FDepthStencilStateOption DepthStencilOption = Material->GetDepthStencilOption();
			if (Command.bDisableDepthTest)
			{
				DepthStencilOption.DepthEnable = false;
			}
			if (Command.bDisableDepthWrite)
			{
				DepthStencilOption.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			}
			Renderer->RenderStateManager->BindState(Renderer->RenderStateManager->GetOrCreateDepthStencilState(DepthStencilOption));
		}
		else
		{
			Renderer->RenderStateManager->BindState(Material->GetDepthStencilState());
		}

		Renderer->RenderStateManager->BindState(Material->GetBlendState());

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

