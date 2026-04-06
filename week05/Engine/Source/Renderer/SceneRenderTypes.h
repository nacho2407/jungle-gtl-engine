#pragma once

#include "CoreMinimal.h"
#include "Renderer/RenderCommand.h"

struct FRenderMesh;
class FMaterial;

struct ENGINE_API FMeshRenderItem
{
	FMaterial* Material = nullptr;
	FRenderMesh* RenderMesh = nullptr;
	FMatrix WorldMatrix = FMatrix::Identity;
	uint32 IndexStart = 0;
	uint32 IndexCount = 0;
	uint32 SectionIndex = 0;
	ERenderPass RenderPass = ERenderPass::Opaque;
	bool bStaticMesh = false;
	uint32 StaticMeshOcclusionCandidateIndex = GInvalidOcclusionCandidateIndex;
};

struct ENGINE_API FMeshDrawCommand
{
	FMaterial* Material = nullptr;
	FRenderMesh* RenderMesh = nullptr;
	uint32 IndexStart = 0;
	uint32 IndexCount = 0;
	uint32 SectionIndex = 0;
	uint32 ObjectUniformAllocation = 0;
	uint64 SubmissionOrder = 0;
	uint64 MaterialKey = 0;
	uint64 MeshKey = 0;
	bool bStaticMesh = false;
	uint32 StaticMeshOcclusionCandidateIndex = GInvalidOcclusionCandidateIndex;
};
