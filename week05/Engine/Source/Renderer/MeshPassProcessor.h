#pragma once

#include "CoreMinimal.h"
#include "Renderer/RenderCommand.h"

struct FRasterizerState;
struct FDepthStencilState;
struct FBlendState;
struct FRenderMesh;
class FMaterial;
class FObjectUniformStream;
class FRenderer;
struct FSceneFramePacket;

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
	ERenderPass RenderPass = ERenderPass::World;
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
	ERenderPass RenderPass = ERenderPass::World;
	bool bDisableDepthTest = false;
	bool bDisableDepthWrite = false;
	bool bDisableCulling = false;
	uint64 SubmissionOrder = 0;
	uint64 PipelineStateKey = 0;
	uint64 MaterialKey = 0;
	uint64 MeshKey = 0;
	uint64 MaterialMeshKey = 0;
	FRasterizerState* RasterizerState = nullptr;
	FDepthStencilState* DepthStencilState = nullptr;
	FBlendState* BlendState = nullptr;
};

class ENGINE_API FMeshPassProcessor
{
public:
	void BuildMeshDrawCommands(const TArray<FMeshBatch>& InMeshBatches, const FRenderCommand* InCommandOverride, FRenderer& Renderer, FSceneFramePacket& InOutPacket, FObjectUniformStream& ObjectUniformStream, uint64& InOutSubmissionOrder) const;

private:
	uint64 BuildPipelineStateKey(const FMaterial* InMaterial, const FMeshBatch& InMeshBatch) const;
};
