#include "Renderer/SceneRenderer.h"

#include <algorithm>

#include "Core/Engine.h"
#include "Renderer/Material.h"
#include "Renderer/ObjectUniformStream.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/RenderMesh.h"
#include "Renderer/Renderer.h"
#include "Renderer/SceneProxy.h"

namespace
{
	FRenderPassState MakeDefaultPassState(ERenderPass RenderPass)
	{
		FRenderPassState PassState = {};

		switch (RenderPass)
		{
		case ERenderPass::Opaque:
			PassState.RasterizerState.FillMode = D3D11_FILL_SOLID;
			PassState.RasterizerState.CullMode = D3D11_CULL_BACK;
			PassState.DepthStencilState.DepthEnable = true;
			PassState.DepthStencilState.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			PassState.DepthStencilState.DepthFunc = D3D11_COMPARISON_LESS;
			break;

		case ERenderPass::Alpha:
			PassState.RasterizerState.FillMode = D3D11_FILL_SOLID;
			PassState.RasterizerState.CullMode = D3D11_CULL_NONE;
			PassState.DepthStencilState.DepthEnable = true;
			PassState.DepthStencilState.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			PassState.DepthStencilState.DepthFunc = D3D11_COMPARISON_LESS;
			PassState.BlendState.BlendEnable = true;
			PassState.BlendState.SrcBlend = D3D11_BLEND_SRC_ALPHA;
			PassState.BlendState.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			PassState.BlendState.BlendOp = D3D11_BLEND_OP_ADD;
			PassState.BlendState.SrcBlendAlpha = D3D11_BLEND_ONE;
			PassState.BlendState.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
			PassState.BlendState.BlendOpAlpha = D3D11_BLEND_OP_ADD;
			break;

		case ERenderPass::NoDepth:
		case ERenderPass::UI:
			PassState.RasterizerState.FillMode = D3D11_FILL_SOLID;
			PassState.RasterizerState.CullMode = D3D11_CULL_NONE;
			PassState.DepthStencilState.DepthEnable = false;
			PassState.DepthStencilState.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			PassState.BlendState.BlendEnable = true;
			PassState.BlendState.SrcBlend = D3D11_BLEND_SRC_ALPHA;
			PassState.BlendState.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			PassState.BlendState.BlendOp = D3D11_BLEND_OP_ADD;
			PassState.BlendState.SrcBlendAlpha = D3D11_BLEND_ONE;
			PassState.BlendState.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
			PassState.BlendState.BlendOpAlpha = D3D11_BLEND_OP_ADD;
			break;

		default:
			break;
		}

		return PassState;
	}

	void ApplyQueuePassOverrides(const FRenderCommandQueue& Queue, FSceneRenderFrame& OutFrame)
	{
		if (Queue.bOpaqueWireframe)
		{
			FRenderPassState& OpaquePassState = OutFrame.GetPassState(ERenderPass::Opaque);
			OpaquePassState.RasterizerState.FillMode = D3D11_FILL_WIREFRAME;
		}
	}

	ERenderPass ResolveRenderPass(const FMeshRenderItem& RenderItem, const FRenderCommand* CommandOverride)
	{
		if (CommandOverride && CommandOverride->bOverrideRenderPass)
		{
			return CommandOverride->RenderPass;
		}

		return RenderItem.RenderPass;
	}

	FMaterial* ResolveMaterial(const FMeshRenderItem& RenderItem, const FRenderCommand* CommandOverride, FRenderer& Renderer)
	{
		if (CommandOverride && CommandOverride->Material)
		{
			return CommandOverride->Material;
		}

		return RenderItem.Material ? RenderItem.Material : Renderer.GetDefaultMaterial();
	}

	void BuildDrawCommands(
		const TArray<FMeshRenderItem>& RenderItems,
		const FRenderCommand* CommandOverride,
		FRenderer& Renderer,
		FSceneRenderFrame& OutFrame,
		FObjectUniformStream& ObjectUniformStream,
		uint64& InOutSubmissionOrder)
	{
		bool bHasCachedObjectAllocation = false;
		FMatrix CachedWorldMatrix = FMatrix::Identity;
		uint32 CachedObjectAllocation = 0;

		for (const FMeshRenderItem& RenderItem : RenderItems)
		{
			if (!RenderItem.RenderMesh)
			{
				continue;
			}

			FMaterial* Material = ResolveMaterial(RenderItem, CommandOverride, Renderer);
			const ERenderPass RenderPass = ResolveRenderPass(RenderItem, CommandOverride);
			if (!Material)
			{
				continue;
			}

			FMeshDrawCommand DrawCommand = {};
			DrawCommand.Material = Material;
			DrawCommand.RenderMesh = RenderItem.RenderMesh;
			DrawCommand.IndexStart = RenderItem.IndexStart;
			DrawCommand.IndexCount = RenderItem.IndexCount;
			DrawCommand.SectionIndex = RenderItem.SectionIndex;
			DrawCommand.SubmissionOrder = InOutSubmissionOrder++;
			DrawCommand.MaterialKey = Material->GetSortId();
			DrawCommand.MeshKey = RenderItem.RenderMesh->GetSortId();

			if (bHasCachedObjectAllocation && RenderItem.WorldMatrix == CachedWorldMatrix)
			{
				DrawCommand.ObjectUniformAllocation = CachedObjectAllocation;
			}
			else
			{
				DrawCommand.ObjectUniformAllocation = ObjectUniformStream.AllocateWorldMatrix(RenderItem.WorldMatrix);
				CachedWorldMatrix = RenderItem.WorldMatrix;
				CachedObjectAllocation = DrawCommand.ObjectUniformAllocation;
				bHasCachedObjectAllocation = true;
			}

			OutFrame.RegisterMeshUpload(RenderItem.RenderMesh);
			OutFrame.GetPassQueue(RenderPass).push_back(DrawCommand);
		}
	}

	bool CompareDrawCommands(const FMeshDrawCommand& A, const FMeshDrawCommand& B)
	{
		if (A.MaterialKey != B.MaterialKey) return A.MaterialKey < B.MaterialKey;
		if (A.MeshKey != B.MeshKey) return A.MeshKey < B.MeshKey;
		if (A.IndexStart != B.IndexStart) return A.IndexStart < B.IndexStart;
		return A.SubmissionOrder < B.SubmissionOrder;
	}
}

void FSceneRenderFrame::Reset()
{
	ViewFamily = {};
	View = {};
	for (TArray<FMeshDrawCommand>& PassQueue : PassQueues)
	{
		PassQueue.clear();
	}
	for (size_t PassIndex = 0; PassIndex < PassStates.size(); ++PassIndex)
	{
		PassStates[PassIndex] = MakeDefaultPassState(static_cast<ERenderPass>(PassIndex));
	}
	MeshUploads.clear();
	OutlineItems.clear();
	// Lights.clear(); GameJam
}

void FSceneRenderFrame::Reserve(size_t InCommandCount)
{
	MeshUploads.reserve(InCommandCount * 2);
	OutlineItems.reserve((std::max)(InCommandCount / 2, static_cast<size_t>(8)));
	for (TArray<FMeshDrawCommand>& PassQueue : PassQueues)
	{
		PassQueue.reserve(InCommandCount);
	}
}

void FSceneRenderFrame::RegisterMeshUpload(FRenderMesh* InMesh)
{
	if (!InMesh || !InMesh->NeedsBufferUpload())
	{
		return;
	}

	MeshUploads.push_back(InMesh);
}

void FSceneRenderer::BuildRenderFrame(const FRenderCommandQueue& Queue, FSceneRenderFrame& OutFrame) const
{
	OutFrame.Reset();
	OutFrame.Reserve(Queue.Commands.size());
	BuildViewInfo(Queue, OutFrame);
	ApplyQueuePassOverrides(Queue, OutFrame);

	if (!Queue.OutlineItems.empty())
	{
		OutFrame.OutlineItems.reserve(Queue.OutlineItems.size());
		for (const FOutlineRenderItem& Item : Queue.OutlineItems)
		{
			if (!Item.Mesh)
			{
				continue;
			}

			OutFrame.OutlineItems.push_back(Item);
			OutFrame.RegisterMeshUpload(Item.Mesh);
		}
	}

	if (!Renderer || !Renderer->ObjectUniformStream)
	{
		return;
	}

	Renderer->ObjectUniformStream->Reset();

	TArray<FMeshRenderItem> CollectedRenderItems;
	CollectedRenderItems.reserve(1);
	uint64 SubmissionOrder = 0;

	for (const FRenderCommand& Command : Queue.Commands)
	{
		CollectedRenderItems.clear();

		if (Command.SceneProxy)
		{
			Command.SceneProxy->CollectMeshBatches(OutFrame.View, *Renderer, CollectedRenderItems);
			BuildDrawCommands(CollectedRenderItems, &Command, *Renderer, OutFrame, *Renderer->ObjectUniformStream, SubmissionOrder);
			continue;
		}

		AppendDirectRenderItem(Command, CollectedRenderItems);
		BuildDrawCommands(CollectedRenderItems, nullptr, *Renderer, OutFrame, *Renderer->ObjectUniformStream, SubmissionOrder);
	}

	if (!OutFrame.MeshUploads.empty())
	{
		std::sort(OutFrame.MeshUploads.begin(), OutFrame.MeshUploads.end());
		const auto NewEnd = std::unique(OutFrame.MeshUploads.begin(), OutFrame.MeshUploads.end());
		OutFrame.MeshUploads.erase(NewEnd, OutFrame.MeshUploads.end());
	}

	if (OutFrame.GetPassQueue(ERenderPass::Opaque).size() > 1)
	{
		TArray<FMeshDrawCommand>& OpaqueCommands = OutFrame.GetPassQueue(ERenderPass::Opaque);
		std::sort(OpaqueCommands.begin(), OpaqueCommands.end(), CompareDrawCommands);
	}
}

void FSceneRenderer::BuildViewInfo(const FRenderCommandQueue& Queue, FSceneRenderFrame& OutFrame) const
{
	OutFrame.ViewFamily.Time = GEngine ? static_cast<float>(GEngine->GetTimer().GetTotalTime()) : 0.0f;
	OutFrame.ViewFamily.DeltaTime = GEngine ? GEngine->GetDeltaTime() : 0.0f;

	FSceneView View = {};
	View.ViewMatrix = Queue.ViewMatrix;
	View.ProjectionMatrix = Queue.ProjectionMatrix;
	View.ShowFlags = Queue.ShowFlags;
	OutFrame.View.Initialize(OutFrame.ViewFamily, View);
}

void FSceneRenderer::AppendDirectRenderItem(const FRenderCommand& Command, TArray<FMeshRenderItem>& OutRenderItems) const
{
	if (!Command.RenderMesh)
	{
		return;
	}

	FMeshRenderItem RenderItem = {};
	RenderItem.Material = Command.Material ? Command.Material : (Renderer ? Renderer->GetDefaultMaterial() : nullptr);
	RenderItem.RenderMesh = Command.RenderMesh;
	RenderItem.WorldMatrix = Command.WorldMatrix;
	RenderItem.IndexStart = Command.IndexStart;
	RenderItem.IndexCount = Command.IndexCount;
	RenderItem.RenderPass = Command.bOverrideRenderPass ? Command.RenderPass : ERenderPass::Opaque;
	OutRenderItems.push_back(RenderItem);
}
