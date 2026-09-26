#pragma once
#include "CoreMinimal.h"
#include "EngineAPI.h"
#include "Math/Vector.h"
#include "Renderer/Feature/DebugLineRenderFeature.h"
#include "../Math/Vector.h"

class FShowFlags;
class UWorld;

// 디버그 라인 요청으로 변환되기 전, 엔진 쪽에서 임시로 보관하는 선분 데이터다.
struct FDebugLine
{
    FVector Start;
    FVector End;
    FVector4 Color;
};

// 디버그 큐브 요청으로 변환되기 전, 엔진 쪽에서 임시로 보관하는 박스 데이터다.
struct FDebugCube
{
    FVector Center;
    FVector Extent;
    FVector4 Color;
};

/**
 * 게임/에디터 공통 디버그 도형 수집기다.
 * 실제 GPU 렌더링은 하지 않고, 프레임 끝에 feature가 소비할 request 데이터만 만든다.
 */
class ENGINE_API FDebugDrawManager
{
public:
	// 디버그 선 하나를 현재 프레임 큐에 추가한다.
	void DrawLine(const FVector& Start, const FVector& End, const FVector4& Color);
	// 디버그 박스 하나를 현재 프레임 큐에 추가한다.
	void DrawCube(const FVector& Center, const FVector& Extent, const FVector4& Color);
	// 월드 축 가시화를 켠다.
	void DrawWorldAxis(float Length = 1000.f);

	// 누적된 디버그 도형을 렌더 요청 구조로 변환한다.
	void BuildRenderRequest(const FShowFlags& ShowFlags, UWorld* World, FDebugLineRenderRequest& OutRequest) const;
	// 현재 프레임에 모인 모든 디버그 도형을 비운다.
	void Clear();
private:
	TArray<FDebugLine> Lines;
	TArray<FDebugCube> Cubes;
	bool bDrawWorldAxis = false;
	// 충돌 바운드를 순회해 디버그 라인 요청에 추가한다.
	void DrawAllCollisionBounds(UWorld* World, FDebugLineRenderRequest& OutRequest) const;
};
