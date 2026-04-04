#pragma once

#include "CoreMinimal.h"
#include "Renderer/RenderState.h"
#include "Renderer/SceneRenderTypes.h"
#include "Renderer/ViewInfo.h"
#include <array>

class FRenderer;
struct FRenderCommand;
struct FRenderCommandQueue;

struct ENGINE_API FRenderPassState
{
	FRasterizerStateOption RasterizerState;
	FDepthStencilStateOption DepthStencilState;
	FBlendStateOption BlendState;
};

// GameJam
//struct ENGINE_API FLightSceneData
//{
//	FVector Position = FVector::ZeroVector;
//	FVector4 Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
//	float Intensity = 1.0f;
//	float Padding[3] = {};
//};

struct ENGINE_API FSceneRenderFrame
{
	using FPassQueueArray = TStaticArray<TArray<FMeshDrawCommand>, static_cast<size_t>(ERenderPass::Count)>;
	using FPassStateArray = TStaticArray<FRenderPassState, static_cast<size_t>(ERenderPass::Count)>;

	FSceneViewFamily ViewFamily;
	FViewInfo View;
	FPassQueueArray PassQueues;
	FPassStateArray PassStates;
	TArray<FRenderMesh*> MeshUploads;
	TArray<FOutlineRenderItem> OutlineItems;
	// TArray<FLightSceneData> Lights; GameJam

	void Reset();
	void Reserve(size_t InCommandCount);
	void RegisterMeshUpload(FRenderMesh* InMesh);
	TArray<FMeshDrawCommand>& GetPassQueue(ERenderPass InRenderPass) { return PassQueues[static_cast<size_t>(InRenderPass)]; }
	const TArray<FMeshDrawCommand>& GetPassQueue(ERenderPass InRenderPass) const { return PassQueues[static_cast<size_t>(InRenderPass)]; }
	FRenderPassState& GetPassState(ERenderPass InRenderPass) { return PassStates[static_cast<size_t>(InRenderPass)]; }
	const FRenderPassState& GetPassState(ERenderPass InRenderPass) const { return PassStates[static_cast<size_t>(InRenderPass)]; }
};

class ENGINE_API FSceneRenderer
{
public:
	explicit FSceneRenderer(FRenderer* InRenderer = nullptr)
		: Renderer(InRenderer)
	{
	}

	void SetRenderer(FRenderer* InRenderer) { Renderer = InRenderer; }
	void BuildRenderFrame(const FRenderCommandQueue& Queue, FSceneRenderFrame& OutFrame) const;

private:
	void BuildViewInfo(const FRenderCommandQueue& Queue, FSceneRenderFrame& OutFrame) const;
	void AppendDirectRenderItem(const FRenderCommand& Command, TArray<FMeshRenderItem>& OutRenderItems) const;

private:
	FRenderer* Renderer = nullptr;
};
