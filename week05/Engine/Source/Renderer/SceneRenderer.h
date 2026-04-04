#pragma once

#include "CoreMinimal.h"
#include "Renderer/MeshPassProcessor.h"
#include "Renderer/PassCommandQueues.h"
#include "Renderer/ViewInfo.h"

class FRenderer;
struct FRenderCommand;
struct FRenderCommandQueue;

struct ENGINE_API FLightSceneData
{
	FVector Position = FVector::ZeroVector;
	FVector4 Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	float Intensity = 1.0f;
	float Padding[3] = {};
};

struct ENGINE_API FSceneFramePacket
{
	FSceneViewFamily ViewFamily;
	FViewInfo View;
	FPassCommandQueues PassCommandQueues;
	TArray<FRenderMesh*> MeshUploads;
	TArray<FOutlineRenderItem> OutlineItems;
	TArray<FLightSceneData> Lights;

	void Reset();
	void Reserve(size_t InCommandCount);
	void RegisterMeshUpload(FRenderMesh* InMesh);
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
