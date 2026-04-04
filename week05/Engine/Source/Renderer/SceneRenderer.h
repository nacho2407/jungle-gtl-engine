#pragma once

#include "CoreMinimal.h"
#include "Renderer/MeshPassProcessor.h"
#include "Renderer/ViewInfo.h"
#include <array>

class FRenderer;
struct FRenderCommand;
struct FRenderCommandQueue;

// GameJam
//struct ENGINE_API FLightSceneData
//{
//	FVector Position = FVector::ZeroVector;
//	FVector4 Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
//	float Intensity = 1.0f;
//	float Padding[3] = {};
//};

struct ENGINE_API FSceneFramePacket
{
	using FPassQueueArray = std::array<TArray<FMeshDrawCommand>, static_cast<size_t>(ERenderPass::Count)>;

	FSceneViewFamily ViewFamily;
	FViewInfo View;
	FPassQueueArray PassQueues;
	TArray<FRenderMesh*> MeshUploads;
	TArray<FOutlineRenderItem> OutlineItems;
	// TArray<FLightSceneData> Lights; GameJam

	void Reset();
	void Reserve(size_t InCommandCount);
	void RegisterMeshUpload(FRenderMesh* InMesh);
	TArray<FMeshDrawCommand>& GetPassQueue(ERenderPass InRenderPass) { return PassQueues[static_cast<size_t>(InRenderPass)]; }
	const TArray<FMeshDrawCommand>& GetPassQueue(ERenderPass InRenderPass) const { return PassQueues[static_cast<size_t>(InRenderPass)]; }
};

class ENGINE_API FSceneRenderer
{
public:
	explicit FSceneRenderer(FRenderer* InRenderer = nullptr)
		: Renderer(InRenderer)
	{
	}

	void SetRenderer(FRenderer* InRenderer) { Renderer = InRenderer; }
	void BuildFramePacket(const FRenderCommandQueue& Queue, FSceneFramePacket& OutPacket) const;

private:
	void BuildViewInfo(const FRenderCommandQueue& Queue, FSceneFramePacket& OutPacket) const;
	void AppendLegacyMeshBatch(const FRenderCommand& Command, TArray<FMeshBatch>& OutMeshBatches) const;

private:
	FRenderer* Renderer = nullptr;
	FMeshPassProcessor MeshPassProcessor;
};
