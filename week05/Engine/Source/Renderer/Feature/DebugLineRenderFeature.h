#pragma once

#include "CoreMinimal.h"
#include <d3d11.h>

class FRenderer;
struct FVertex;

struct ENGINE_API FDebugLineRenderItem
{
	// 월드 공간 기준 선의 시작점이다.
	FVector Start = FVector::ZeroVector;
	// 월드 공간 기준 선의 끝점이다.
	FVector End = FVector::ZeroVector;
	// 선 하나에 적용할 색상이다.
	FVector4 Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
};

struct ENGINE_API FDebugLineRenderRequest
{
	// 이번 요청에서 그릴 모든 디버그 라인 목록이다.
	TArray<FDebugLineRenderItem> Lines;

	// 요청에 쌓인 모든 라인을 비운다.
	void Clear()
	{
		Lines.clear();
	}

	// 그릴 라인이 하나도 없으면 true를 반환한다.
	bool IsEmpty() const
	{
		return Lines.empty();
	}
};

class ENGINE_API FDebugLineRenderFeature
{
public:
	~FDebugLineRenderFeature();

	// 디버그 라인 요청에 선 하나를 추가한다.
	static void AddLine(FDebugLineRenderRequest& Request, const FVector& Start, const FVector& End, const FVector4& Color);
	// AABB를 12개의 선분으로 풀어 요청에 추가한다.
	static void AddCube(FDebugLineRenderRequest& Request, const FVector& Center, const FVector& BoxExtent, const FVector4& Color);

	// 요청에 담긴 디버그 라인을 업로드하고 그린다.
	bool Render(FRenderer& Renderer, const FDebugLineRenderRequest& Request);
	// 디버그 라인 렌더링에 쓰는 임시 버텍스 버퍼를 해제한다.
	void Release();

private:
	ID3D11Buffer* LineVertexBuffer = nullptr;
	UINT LineVertexBufferSize = 0;
};
