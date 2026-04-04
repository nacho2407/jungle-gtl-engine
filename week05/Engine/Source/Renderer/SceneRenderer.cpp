#include "Renderer/SceneRenderer.h"

#include <algorithm>

#include "Core/Engine.h"
#include "Renderer/ObjectUniformStream.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/RenderMesh.h"
#include "Renderer/Renderer.h"
#include "Renderer/SceneProxy.h"

void FSceneRenderFrame::Reset()
{
	ViewFamily = {};
	View = {};
	for (TArray<FMeshDrawCommand>& PassQueue : PassQueues)
	{
		PassQueue.clear();
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

void FSceneRenderer::BuildFramePacket(const FRenderCommandQueue& Queue, FSceneRenderFrame& OutPacket) const
{
	OutPacket.Reset();
	OutPacket.Reserve(Queue.Commands.size());
	BuildViewInfo(Queue, OutPacket);

	if (!Queue.OutlineItems.empty())
	{
		OutPacket.OutlineItems.reserve(Queue.OutlineItems.size());
		for (const FOutlineRenderItem& Item : Queue.OutlineItems)
		{
			if (!Item.Mesh)
			{
				continue;
			}

			OutPacket.OutlineItems.push_back(Item);
			OutPacket.RegisterMeshUpload(Item.Mesh);
		}
	}

	if (!Renderer || !Renderer->ObjectUniformStream)
	{
		return;
	}

	Renderer->ObjectUniformStream->Reset();

	TArray<FMeshRenderItem> MeshBatches;
	MeshBatches.reserve(1);
	uint64 SubmissionOrder = 0;

	for (const FRenderCommand& Command : Queue.Commands)
	{
		if (Command.SceneProxy)
		{
			Command.SceneProxy->AppendDrawCommands(Command, OutPacket.View, *Renderer, MeshPassProcessor, OutPacket, *Renderer->ObjectUniformStream, SubmissionOrder);
		}
		else
		{
			MeshBatches.clear();
			AppendLegacyMeshBatch(Command, MeshBatches);
			MeshPassProcessor.BuildMeshDrawCommands(MeshBatches, nullptr, *Renderer, OutPacket, *Renderer->ObjectUniformStream, SubmissionOrder);
		}
	}

	if (!OutPacket.MeshUploads.empty())
	{
		std::sort(OutPacket.MeshUploads.begin(), OutPacket.MeshUploads.end());
		const auto NewEnd = std::unique(OutPacket.MeshUploads.begin(), OutPacket.MeshUploads.end());
		OutPacket.MeshUploads.erase(NewEnd, OutPacket.MeshUploads.end());
	}

	auto PassComparator = [](const FMeshDrawCommand& A, const FMeshDrawCommand& B)
	{
		if (A.PipelineStateKey != B.PipelineStateKey) return A.PipelineStateKey < B.PipelineStateKey;
		if (A.MaterialKey != B.MaterialKey) return A.MaterialKey < B.MaterialKey;
		if (A.MeshKey != B.MeshKey) return A.MeshKey < B.MeshKey;
		if (A.SectionIndex != B.SectionIndex) return A.SectionIndex < B.SectionIndex;
		return A.SubmissionOrder < B.SubmissionOrder;
	};

	if (OutPacket.GetPassQueue(ERenderPass::World).size() > 1)
	{
		TArray<FMeshDrawCommand>& WorldCommands = OutPacket.GetPassQueue(ERenderPass::World);
		std::sort(WorldCommands.begin(), WorldCommands.end(), PassComparator);
	}
	if (OutPacket.GetPassQueue(ERenderPass::NoDepth).size() > 1)
	{
		TArray<FMeshDrawCommand>& NoDepthCommands = OutPacket.GetPassQueue(ERenderPass::NoDepth);
		std::sort(NoDepthCommands.begin(), NoDepthCommands.end(), PassComparator);
	}
}

void FSceneRenderer::BuildViewInfo(const FRenderCommandQueue& Queue, FSceneRenderFrame& OutPacket) const
{
	OutPacket.ViewFamily.Time = GEngine ? static_cast<float>(GEngine->GetTimer().GetTotalTime()) : 0.0f;
	OutPacket.ViewFamily.DeltaTime = GEngine ? GEngine->GetDeltaTime() : 0.0f;

	FSceneView View = {};
	View.ViewMatrix = Queue.ViewMatrix;
	View.ProjectionMatrix = Queue.ProjectionMatrix;
	View.ShowFlags = Queue.ShowFlags;
	OutPacket.View.Initialize(OutPacket.ViewFamily, View);
}

void FSceneRenderer::AppendLegacyMeshBatch(const FRenderCommand& Command, TArray<FMeshRenderItem>& OutMeshBatches) const
{
	if (!Command.RenderMesh)
	{
		return;
	}

	FMeshRenderItem MeshBatch = {};
	MeshBatch.Material = Command.Material ? Command.Material : (Renderer ? Renderer->GetDefaultMaterial() : nullptr);
	MeshBatch.RenderMesh = Command.RenderMesh;
	MeshBatch.WorldMatrix = Command.WorldMatrix;
	MeshBatch.IndexStart = Command.IndexStart;
	MeshBatch.IndexCount = Command.IndexCount;
	MeshBatch.RenderPass = Command.RenderPass;
	MeshBatch.bDisableDepthTest = Command.bDisableDepthTest;
	MeshBatch.bDisableDepthWrite = Command.bDisableDepthWrite;
	MeshBatch.bDisableCulling = Command.bDisableCulling;
	OutMeshBatches.push_back(MeshBatch);
}
