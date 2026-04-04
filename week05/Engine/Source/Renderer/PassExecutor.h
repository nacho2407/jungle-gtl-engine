#pragma once

#include "CoreMinimal.h"
#include "Renderer/MeshPassProcessor.h"

class FRenderer;
struct FSceneRenderFrame;

class ENGINE_API FPassExecutor
{
public:
	explicit FPassExecutor(FRenderer* InRenderer = nullptr)
		: Renderer(InRenderer)
	{
	}

	void SetRenderer(FRenderer* InRenderer) { Renderer = InRenderer; }
	void Execute(const FSceneRenderFrame& Packet) const;

private:
	void UpdateUploadedMeshes(const FSceneRenderFrame& Packet) const;
	void FlushDirtyMaterialConstantBuffers(const FSceneRenderFrame& Packet) const;
	void ExecuteQueue(const TArray<FMeshDrawCommand>& InCommands) const;

private:
	FRenderer* Renderer = nullptr;
};
