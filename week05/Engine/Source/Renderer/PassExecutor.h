#pragma once

#include "CoreMinimal.h"
#include "Renderer/MeshPassProcessor.h"

class FRenderer;
struct FSceneFramePacket;

class ENGINE_API FPassExecutor
{
public:
	explicit FPassExecutor(FRenderer* InRenderer = nullptr)
		: Renderer(InRenderer)
	{
	}

	void SetRenderer(FRenderer* InRenderer) { Renderer = InRenderer; }
	void Execute(const FSceneFramePacket& Packet) const;

private:
	void UpdateUploadedMeshes(const FSceneFramePacket& Packet) const;
	void FlushDirtyMaterialConstantBuffers(const FSceneFramePacket& Packet) const;
	void ExecuteQueue(const TArray<FMeshDrawCommand>& InCommands) const;

private:
	FRenderer* Renderer = nullptr;
};
