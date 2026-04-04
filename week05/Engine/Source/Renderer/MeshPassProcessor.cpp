#include "Renderer/MeshPassProcessor.h"

#include "Renderer/Material.h"
#include "Renderer/ObjectUniformStream.h"
#include "Renderer/PassCommandQueues.h"
#include "Renderer/RenderMesh.h"
#include "Renderer/RenderStateManager.h"
#include "Renderer/Renderer.h"
#include "Renderer/SceneRenderer.h"

namespace
{
	ERenderPass ResolveRenderPass(const FMeshBatch& InMeshBatch, const FRenderCommand* InCommandOverride)
	{
		if (!InCommandOverride)
		{
			return InMeshBatch.RenderPass;
		}

		if (InCommandOverride->RenderPass != ERenderPass::World)
		{
			return InCommandOverride->RenderPass;
		}

		return InMeshBatch.RenderPass;
	}

	FMaterial* ResolveMaterial(const FMeshBatch& InMeshBatch, const FRenderCommand* InCommandOverride, FRenderer& Renderer)
	{
		if (InCommandOverride && InCommandOverride->Material)
		{
			return InCommandOverride->Material;
		}

		return InMeshBatch.Material ? InMeshBatch.Material : Renderer.GetDefaultMaterial();
	}

	bool ResolveDisableDepthTest(const FMeshBatch& InMeshBatch, const FRenderCommand* InCommandOverride)
	{
		return InMeshBatch.bDisableDepthTest || (InCommandOverride && InCommandOverride->bDisableDepthTest);
	}

	bool ResolveDisableDepthWrite(const FMeshBatch& InMeshBatch, const FRenderCommand* InCommandOverride)
	{
		return InMeshBatch.bDisableDepthWrite || (InCommandOverride && InCommandOverride->bDisableDepthWrite);
	}

	bool ResolveDisableCulling(const FMeshBatch& InMeshBatch, const FRenderCommand* InCommandOverride)
	{
		return InMeshBatch.bDisableCulling || (InCommandOverride && InCommandOverride->bDisableCulling);
	}
}

void FMeshPassProcessor::BuildMeshDrawCommands(const TArray<FMeshBatch>& InMeshBatches, const FRenderCommand* InCommandOverride, FRenderer& Renderer, FSceneFramePacket& InOutPacket, FObjectUniformStream& ObjectUniformStream, uint64& InOutSubmissionOrder) const
{
	FRenderStateManager* RenderStateManager = Renderer.GetRenderStateManager().get();
	if (!RenderStateManager)
	{
		return;
	}

	bool bHasCachedObjectAllocation = false;
	FMatrix CachedWorldMatrix = FMatrix::Identity;
	uint32 CachedObjectAllocation = 0;

	for (const FMeshBatch& MeshBatch : InMeshBatches)
	{
		if (!MeshBatch.Element.RenderMesh)
		{
			continue;
		}

		FMaterial* Material = ResolveMaterial(MeshBatch, InCommandOverride, Renderer);
		if (!Material)
		{
			continue;
		}

		const bool bDisableDepthTest = ResolveDisableDepthTest(MeshBatch, InCommandOverride);
		const bool bDisableDepthWrite = ResolveDisableDepthWrite(MeshBatch, InCommandOverride);
		const bool bDisableCulling = ResolveDisableCulling(MeshBatch, InCommandOverride);
		const ERenderPass RenderPass = ResolveRenderPass(MeshBatch, InCommandOverride);

		FMeshBatch EffectiveMeshBatch = MeshBatch;
		EffectiveMeshBatch.Material = Material;
		EffectiveMeshBatch.RenderPass = RenderPass;
		EffectiveMeshBatch.bDisableDepthTest = bDisableDepthTest;
		EffectiveMeshBatch.bDisableDepthWrite = bDisableDepthWrite;
		EffectiveMeshBatch.bDisableCulling = bDisableCulling;

		FMeshDrawCommand DrawCommand = {};
		DrawCommand.Material = Material;
		DrawCommand.RenderMesh = MeshBatch.Element.RenderMesh;
		DrawCommand.IndexStart = MeshBatch.Element.IndexStart;
		DrawCommand.IndexCount = MeshBatch.Element.IndexCount;
		DrawCommand.SectionIndex = MeshBatch.Element.SectionIndex;
		DrawCommand.RenderPass = RenderPass;
		DrawCommand.bDisableDepthTest = bDisableDepthTest;
		DrawCommand.bDisableDepthWrite = bDisableDepthWrite;
		DrawCommand.bDisableCulling = bDisableCulling;
		DrawCommand.SubmissionOrder = InOutSubmissionOrder++;
		DrawCommand.PipelineStateKey = BuildPipelineStateKey(Material, EffectiveMeshBatch);
		DrawCommand.MaterialKey = Material->GetSortId();
		DrawCommand.MeshKey = MeshBatch.Element.RenderMesh->GetSortId();
		DrawCommand.MaterialMeshKey = (DrawCommand.MaterialKey << 32ull) | (DrawCommand.MeshKey & 0xFFFFFFFFull);
		DrawCommand.RasterizerState = Material->ResolveRasterizerState(*RenderStateManager, bDisableCulling);
		DrawCommand.DepthStencilState = Material->ResolveDepthStencilState(*RenderStateManager, bDisableDepthTest, bDisableDepthWrite);
		DrawCommand.BlendState = Material->GetBlendState().get();

		if (bHasCachedObjectAllocation && MeshBatch.Element.WorldMatrix == CachedWorldMatrix)
		{
			DrawCommand.ObjectUniformAllocation = CachedObjectAllocation;
		}
		else
		{
			DrawCommand.ObjectUniformAllocation = ObjectUniformStream.AllocateWorldMatrix(MeshBatch.Element.WorldMatrix);
			CachedWorldMatrix = MeshBatch.Element.WorldMatrix;
			CachedObjectAllocation = DrawCommand.ObjectUniformAllocation;
			bHasCachedObjectAllocation = true;
		}

		InOutPacket.RegisterMeshUpload(MeshBatch.Element.RenderMesh);
		InOutPacket.PassCommandQueues.GetQueue(DrawCommand.RenderPass).push_back(DrawCommand);
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
