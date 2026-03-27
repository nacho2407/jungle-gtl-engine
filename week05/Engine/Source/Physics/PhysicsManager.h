#pragma once
#include "Math/Vector.h"

class AActor;
class UScene;

struct FHitResult
{
	AActor* HitActor;
	FVector HitLocation;
};

class CPhysicsManager
{
public:
	/**
	 * 
	 * 
	 * \param Scene: Actor 데이터 참조용
	 * \param Start: Line 시작점
	 * \param End: Line 끝점
	 * \param OutHit: 처음으로 Hit 된 대상에 대한 정보 (HitActor, HitLocation, ...)
	 * \return 
	 */
	bool Linetrace(const UScene* Scene, const FVector& Start, const FVector& End, FHitResult& OutHit);
private:
};
