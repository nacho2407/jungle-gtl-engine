#pragma once

#include "Core/CollisionTypes.h"
#include "Particle/ParticleTypes.h"

class AActor;
class UWorld;

/**
 * IParticleEmitterInstanceOwner
 * - EmitterInstance가 자신을 보유한 Component 정보에 접근하기 위한 Interface
 * - 필요한 API가 있다면 InstanceOwner에 추가하고 cpp에 구현
 */
class IParticleEmitterInstanceOwner
{
public:
	virtual ~IParticleEmitterInstanceOwner() = default;

	virtual UWorld* GetWorld() const = 0;
	virtual FVector GetWorldLocation() const = 0;
	virtual FMatrix GetComponentToWorld() const = 0;

	/**
	 * @brief source actor ignore 정책에 사용할 PSC 소유 actor 조회
	 */
	virtual AActor* GetSourceActor() const = 0;

	/**
	 * @brief particle 이동 구간을 world Shape query로 검사
	 * @param CollisionShape line 또는 이동 sphere query 형상
	 */
	virtual bool ParticleLineCheck(
		FHitResult& Hit,
		AActor* SourceActor,
		const FVector& EndWS,
		const FVector& StartWS,
		const FCollisionShape& CollisionShape) = 0;

	/**
	 * @brief 내부 receiver 입력 event 저장
	 */
	virtual void AddParticleEvent(const FParticleEventPayload& Event) = 0;
};
