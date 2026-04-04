#pragma once

#include "CoreMinimal.h"
#include "Renderer/RenderCommand.h"

struct FRenderMesh;
class FMaterial;
class FObjectUniformStream;
struct FSceneFramePacket;

enum class EMeshPass : uint8
{
	Base,
	Overlay,
	UI,
	OutlineMask,
	OutlineComposite,
};

struct ENGINE_API FMeshBatchElement
{
	FRenderMesh* RenderMesh = nullptr;
	FMatrix WorldMatrix = FMatrix::Identity;
	uint32 IndexStart = 0;
	uint32 IndexCount = 0;
	uint32 SectionIndex = 0;
};

struct ENGINE_API FMeshBatch
{
	FMaterial* Material = nullptr;
	FMeshBatchElement Element;
	ERenderLayer RenderLayer = ERenderLayer::Base;
	bool bDisableDepthTest = false;
	bool bDisableDepthWrite = false;
	bool bDisableCulling = false;
};

struct ENGINE_API FMeshDrawCommand
{
	FMaterial* Material = nullptr;
	FRenderMesh* RenderMesh = nullptr;
	uint32 IndexStart = 0;
	uint32 IndexCount = 0;
	uint32 SectionIndex = 0;
	uint32 ObjectUniformAllocation = 0;
	EMeshPass MeshPass = EMeshPass::Base;
	bool bDisableDepthTest = false;
	bool bDisableDepthWrite = false;
	bool bDisableCulling = false;
	uint64 SubmissionOrder = 0;
	uint64 PipelineStateKey = 0;
	uint64 MaterialKey = 0;
	uint64 MeshKey = 0;
	uint64 MaterialMeshKey = 0;
};

class ENGINE_API FMeshPassProcessor
{
public:
	void BuildMeshDrawCommands(const TArray<FMeshBatch>& InMeshBatches, FSceneFramePacket& InOutPacket, FObjectUniformStream& ObjectUniformStream, uint64& InOutSubmissionOrder) const;

private:
	static EMeshPass ResolveMeshPass(ERenderLayer InRenderLayer);
	uint64 BuildPipelineStateKey(const FMaterial* InMaterial, const FMeshBatch& InMeshBatch) const;
};
