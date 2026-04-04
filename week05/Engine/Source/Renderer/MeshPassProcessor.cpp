#include "Renderer/MeshPassProcessor.h"

#include "Renderer/Material.h"
#include "Renderer/ObjectUniformStream.h"
#include "Renderer/PassCommandQueues.h"
#include "Renderer/RenderMesh.h"
#include "Renderer/SceneRenderer.h"

void FMeshPassProcessor::BuildMeshDrawCommands(const TArray<FMeshBatch>& InMeshBatches, FSceneFramePacket& InOutPacket, FObjectUniformStream& ObjectUniformStream, uint64& InOutSubmissionOrder) const
{
	for (const FMeshBatch& MeshBatch : InMeshBatches)
	{
		if (!MeshBatch.Element.RenderMesh)
		{
			continue;
		}

		FMeshDrawCommand DrawCommand = {};
		DrawCommand.Material = MeshBatch.Material;
		DrawCommand.RenderMesh = MeshBatch.Element.RenderMesh;
		DrawCommand.IndexStart = MeshBatch.Element.IndexStart;
		DrawCommand.IndexCount = MeshBatch.Element.IndexCount;
		DrawCommand.SectionIndex = MeshBatch.Element.SectionIndex;
		DrawCommand.MeshPass = ResolveMeshPass(MeshBatch.RenderLayer);
		DrawCommand.bDisableDepthTest = MeshBatch.bDisableDepthTest;
		DrawCommand.bDisableDepthWrite = MeshBatch.bDisableDepthWrite;
		DrawCommand.bDisableCulling = MeshBatch.bDisableCulling;
		DrawCommand.SubmissionOrder = InOutSubmissionOrder++;
		DrawCommand.PipelineStateKey = BuildPipelineStateKey(MeshBatch.Material, MeshBatch);
		DrawCommand.MaterialKey = MeshBatch.Material ? MeshBatch.Material->GetSortId() : 0;
		DrawCommand.MeshKey = MeshBatch.Element.RenderMesh ? MeshBatch.Element.RenderMesh->GetSortId() : 0;
		DrawCommand.MaterialMeshKey = (DrawCommand.MaterialKey << 32ull) | (DrawCommand.MeshKey & 0xFFFFFFFFull);
		DrawCommand.ObjectUniformAllocation = ObjectUniformStream.AllocateWorldMatrix(MeshBatch.Element.WorldMatrix);

		InOutPacket.RegisterMeshUpload(MeshBatch.Element.RenderMesh);
		InOutPacket.PassCommandQueues.GetQueue(DrawCommand.MeshPass).push_back(DrawCommand);
	}
}

EMeshPass FMeshPassProcessor::ResolveMeshPass(ERenderLayer InRenderLayer)
{
	switch (InRenderLayer)
	{
	case ERenderLayer::Overlay:
		return EMeshPass::Overlay;
	case ERenderLayer::UI:
		return EMeshPass::UI;
	case ERenderLayer::OutlineMask:
		return EMeshPass::OutlineMask;
	case ERenderLayer::OutlineComposite:
		return EMeshPass::OutlineComposite;
	case ERenderLayer::Base:
	default:
		return EMeshPass::Base;
	}
}

uint64 FMeshPassProcessor::BuildPipelineStateKey(const FMaterial* InMaterial, const FMeshBatch& InMeshBatch) const
{
	if (!InMaterial)
	{
		return 0;
	}

	return InMaterial->GetPipelineStateKey(
		InMeshBatch.bDisableCulling,
		InMeshBatch.bDisableDepthTest,
		InMeshBatch.bDisableDepthWrite);
}
