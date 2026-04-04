#pragma once

#include "CoreMinimal.h"
#include "Core/ShowFlags.h"
#include <algorithm>
#include <utility>

struct FRenderMesh;
class FMaterial;
class FPrimitiveSceneProxy;

enum class ERenderPass : uint8
{
	World = 0,
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

	ERenderPass RenderPass = ERenderPass::World;
	bool bOverrideRenderPass = false;
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

	FMatrix ViewMatrix = FMatrix::Identity;
	FMatrix ProjectionMatrix = FMatrix::Identity;
	FShowFlags ShowFlags;
	bool bWorldWireframe = false;

	void Reserve(size_t Count)
	{
		Commands.reserve(Count);
		OutlineItems.reserve((std::max)(Count / 2, static_cast<size_t>(8)));
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
		bWorldWireframe = false;
	}
};
