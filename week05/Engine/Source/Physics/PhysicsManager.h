#pragma once
#include "Math/Vector.h"

class AActor;
class ULevel;

struct FHitResult
{
	AActor* HitActor;
	FVector HitLocation;
};

class FPhysicsManager
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
	bool Linetrace(const ULevel* Scene, const FVector& Start, const FVector& End, FHitResult& OutHit);
private:
};
