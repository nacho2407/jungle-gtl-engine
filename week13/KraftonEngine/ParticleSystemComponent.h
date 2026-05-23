#pragma once
#include "Component/PrimitiveComponent.h"
#include "Engine/Particle/DynamicEmitterData.h"

class UParticleSystemComponent : public UPrimitiveComponent
{
public:
	// ================================================================
	// [컴포넌트 팀 구현 필요]
	// 매 틱 파티클 시뮬레이션 결과를 FDynamicEmitterDataBase로 패키징하여 반환.
	// 반환된 포인터는 다음 틱까지 유효해야 함 (프록시가 한 프레임 동안 읽음).
	// ================================================================
	virtual const TArray<FDynamicEmitterDataBase*>& GetDynamicData() const
	{
		// 컴포넌트 구현 전 빈 배열 반환 — 프록시는 렌더링을 건너뜀
		static const TArray<FDynamicEmitterDataBase*> Empty;
		return Empty;
	}

protected:
	// 시뮬레이션 결과를 매 틱 여기에 채워넣을 것
	// 소유권은 컴포넌트가 가짐 (프록시는 포인터만 빌림)
	TArray<FDynamicEmitterDataBase*> CachedDynamicData;
};
