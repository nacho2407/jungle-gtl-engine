#pragma once
#include "Math/Vector.h" // 필요한 최소한의 수학 라이브러리만

struct FHitResult 
{
    class UPrimitiveComponent* HitComponent = nullptr;

    float Distance = FLT_MAX;
	
	//	World 기준
    FVector Location = { 0, 0, 0 };
    FVector Normal = { 0, 0, 0 };
	
    int FaceIndex = -1; 

    bool bHit = false;
	
	void Reset()
	{
		HitComponent = nullptr;
		Distance = FLT_MAX;
		Location = { 0, 0, 0 };
		Normal = { 0, 0, 0 };
		FaceIndex = -1;
		bHit = false;
	}
	
	bool IsValid() const
	{
		return bHit && (HitComponent != nullptr);
	}
};