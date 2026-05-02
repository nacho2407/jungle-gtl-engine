#pragma once
#include "Engine/Core/CoreTypes.h"
#include "Collision/BVH/BVH.h"


class UPrimitiveComponent;
class FScenePrimitiveBVH;
class UShapeComponent;
class FScene;

using FCollisionBVH = TBVH<UPrimitiveComponent*, 8, 16>;

struct FOverlapPair
{
    UPrimitiveComponent* A;
    UPrimitiveComponent* B;

    bool operator==(const FOverlapPair& Other) const
    {
        return (A == Other.A && B == Other.B) || (A == Other.B && B == Other.A);
    }
};

class FCollisionManager
{
public:
    // 컴포넌트가 활성화(BeginPlay 등)될 때 자신을 등록합니다.
    void RegisterComponent(UPrimitiveComponent* Component);
    void UnregisterComponent(UPrimitiveComponent* Component);

    void TickCollision(float DeltaTime, FScene* Scene = nullptr);

	void Reset() { RegisteredComponents.clear(); }

	    void BuildBVH();
    void UpdateCollisionInBVH(UPrimitiveComponent* Component){ if (Component) bNeedsBVHRebuild = true;}

private:
    // BVH 루트 노드
    FCollisionBVH CollisionTree;

    // 등록된 모든 콜리전 컴포넌트들
    TArray<UPrimitiveComponent*> RegisteredComponents;

    // 1. 프레임 간 상태 비교를 위한 장부
    TArray<FOverlapPair> PreviousFrameOverlaps;
    TArray<FOverlapPair> CurrentFrameOverlaps;

    // 2. 루프가 끝난 뒤 한꺼번에 터뜨릴 지연 이벤트 큐
    TArray<FOverlapPair> PendingBeginOverlaps;
    TArray<FOverlapPair> PendingEndOverlaps;

    // Awake 상태(움직인) 객체들의 목록 (프레임마다 갱신)
    // TArray<UPrimitiveComponent*> AwakeComponents;

    // BVH 업데이트 및 충돌 쿼리 로직
    // void UpdateBVH();

    bool CheckOverlap(UPrimitiveComponent* A, UPrimitiveComponent* B);
    
	bool bNeedsBVHRebuild = true;
};
