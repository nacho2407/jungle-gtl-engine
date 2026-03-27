#pragma once

#include "CoreMinimal.h"

struct FMeshData;
class FMaterial;

enum class ERenderLayer {
	Default,
	Overlay,
	UI,
};

struct ENGINE_API FRenderCommand
{
	FMeshData* MeshData = nullptr;
	FMatrix WorldMatrix;
	FMaterial* Material = nullptr;
	uint64 SortKey = 0;

	ERenderLayer RenderLayer = ERenderLayer::Default;
	bool bDisableDepthTest = false;
	bool bDisableDepthWrite = false;
	bool bDisableCulling = false;

	static uint64 MakeSortKey(const FMaterial* InMaterial, const FMeshData* InMeshData);
};

/**
 * 한 프레임 동안 수집된 모든 렌더링 명령을 담는 큐
 */
struct ENGINE_API FRenderCommandQueue
{
	/** 일반 메시 렌더링 명령 목록 (텍스트, SubUV 포함 통합) */
	TArray<FRenderCommand> Commands;

	/** 프레임의 카메라 행렬 */
	FMatrix ViewMatrix;
	FMatrix ProjectionMatrix;

	void Reserve(size_t Count)
	{
		Commands.reserve(Count);
	}

	void AddCommand(const FRenderCommand& Cmd)
	{
		Commands.push_back(Cmd);
	}

	/** 큐 초기화 */
	void Clear()
	{
		Commands.clear();
	}
};
