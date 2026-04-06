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

inline uint64 MakeStaticMeshOcclusionMatchKey(uint32 CandidateId, const FRenderMesh* RenderMesh)
{
	uint64 Key = static_cast<uint64>(CandidateId);
	const uint64 MeshKey = static_cast<uint64>(reinterpret_cast<uintptr_t>(RenderMesh));
	Key ^= MeshKey + 0x9e3779b97f4a7c15ull + (Key << 6) + (Key >> 2);
	return Key;
}

struct ENGINE_API FStaticMeshOcclusionCandidate
{
	uint32 CandidateId = 0;
	const UStaticMeshComponent* Component = nullptr;
	const FPrimitiveSceneProxy* SceneProxy = nullptr;
	UStaticMesh* StaticMesh = nullptr;
	FRenderMesh* RenderMesh = nullptr;
	uint64 MatchKey = 0;
	FVector BoundsCenter = FVector::ZeroVector;
	float BoundsRadius = 0.0f;
	FVector BoundsExtent = FVector::ZeroVector;
	FMatrix WorldMatrix = FMatrix::Identity;
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
		bOpaqueWireframe = false;
	}
};
