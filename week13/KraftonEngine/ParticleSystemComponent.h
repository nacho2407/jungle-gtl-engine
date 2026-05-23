#pragma once
#include "Component/PrimitiveComponent.h"
#include "Engine/Particle/DynamicEmitterData.h"

class UParticleSystemComponent : public UPrimitiveComponent
{
public:
	// 프록시 작성을 위한 더미용
	// 언제든지 다 밀어버리고 재작성하셔도 됩니다.
	virtual const TArray<FDynamicEmitterDataBase*>& GetDynamicData() const
	{
		static const TArray<FDynamicEmitterDataBase*> Empty;
		return Empty;
	}

protected:
	TArray<FDynamicEmitterDataBase*> CachedDynamicData;
};
