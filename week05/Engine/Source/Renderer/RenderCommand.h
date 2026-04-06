#pragma once

#include "CoreMinimal.h"
#include "Core/ShowFlags.h"
#include <algorithm>
#include <cstdint>
#include <utility>

struct FRenderMesh;
class FMaterial;
class FPrimitiveSceneProxy;
class UStaticMesh;
class UStaticMeshComponent;

constexpr uint32 GInvalidOcclusionCandidateIndex = UINT32_MAX;

inline uint64 MakeStaticMeshOcclusionMatchKey(uint32 CandidateId)
{
	return static_cast<uint64>(CandidateId);
}

struct ENGINE_API FStaticMeshOcclusionCandidate
{
	uint64 MatchKey = 0;
	FVector BoundsCenter = FVector::ZeroVector;
	float BoundsRadius = 0.0f;
	FVector BoundsExtent = FVector::ZeroVector;
};

struct ENGINE_API FStaticMeshOcclusionFrameSnapshot
{
	TArray<uint64> CandidateKeys;

	void Reserve(size_t Count)
	{
		CandidateKeys.reserve(Count);
	}

	void Clear()
	{
		CandidateKeys.clear();
	}
};

enum class ERenderPass : uint8
{
	Opaque = 0,
	Alpha,
	NoDepth,
	UI,
	Count,
};

struct ENGINE_API FRenderCommand
{
	const FPrimitiveSceneProxy* SceneProxy = nullptr;
	FRenderMesh* RenderMesh = nullptr;

	FMatrix WorldMatrix = FMatrix::Identity;
	FMaterial* Material = nullptr;

	uint32 IndexStart = 0;
	uint32 IndexCount = 0;

	ERenderPass RenderPass = ERenderPass::Opaque;
	bool bOverrideRenderPass = false;
	bool bStaticMesh = false;
	uint32 StaticMeshOcclusionCandidateIndex = GInvalidOcclusionCandidateIndex;
};

struct ENGINE_API FOutlineRenderItem
{
	FRenderMesh* Mesh = nullptr;
	FMatrix WorldMatrix = FMatrix::Identity;
};

struct ENGINE_API FRenderCommandQueue
{
	TArray<FRenderCommand> Commands;
	TArray<FOutlineRenderItem> OutlineItems;
	TArray<FStaticMeshOcclusionCandidate> StaticMeshOcclusionCandidates;
	uint32 PreSkippedStaticMeshDrawCallCount = 0;

	FMatrix ViewMatrix = FMatrix::Identity;
	FMatrix ProjectionMatrix = FMatrix::Identity;
	FShowFlags ShowFlags;
	bool bOpaqueWireframe = false;

	void Reserve(size_t Count)
	{
		Commands.reserve(Count);
		OutlineItems.reserve((std::max)(Count / 2, static_cast<size_t>(8)));
		StaticMeshOcclusionCandidates.reserve(Count);
	}

	void AddCommand(const FRenderCommand& Cmd)
	{
		Commands.push_back(Cmd);
	}

	void AddCommand(FRenderCommand&& Cmd)
	{
		Commands.push_back(std::move(Cmd));
	}

	void Clear()
	{
		Commands.clear();
		OutlineItems.clear();
		StaticMeshOcclusionCandidates.clear();
		PreSkippedStaticMeshDrawCallCount = 0;
		bOpaqueWireframe = false;
	}
};
