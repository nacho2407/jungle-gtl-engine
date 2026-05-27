#pragma once

#include "GameFramework/AActor.h"
UCLASS()
class AParticleEventManager : public AActor
{
public:
	GENERATED_BODY(AParticleEventManager, AActor)
};

// 현재 named particle event는 UParticleSystemComponent 내부 buffer에서만 처리
