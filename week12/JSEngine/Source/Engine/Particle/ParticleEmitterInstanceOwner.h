#pragma once

#include "Core/CollisionTypes.h"
#include "Particle/ParticleTypes.h"

class AActor;
class UWorld;
struct FParticleInstanceParameter;

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
	 * @brief 이름이 같은 PSC instance parameter를 조회합니다.
	 *
	 * @param Name 조회할 parameter 이름
	 *
	 * @return 이름이 같은 parameter 포인터. 없으면 nullptr
	 */
	virtual const FParticleInstanceParameter* FindInstanceParameter(const FString& Name) const = 0;

	/**
	 * @brief 이름과 Beam endpoint method에 맞는 world point를 해석합니다.
	 *
	 * @param Name 조회할 parameter 이름
	 *
	 * @param Method Beam endpoint 해석 방식
	 *
	 * @param OutWorldPoint 해석된 world space 위치
	 *
	 * @return 해석 성공 여부
	 */
	virtual bool ResolveParticleInstanceParameterWorldPoint(
		const FString& Name,
		EParticleBeamEndpointMethod Method,
		FVector& OutWorldPoint) const = 0;

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

	virtual void AddCollisionEvent(const FParticleEventCollideData& Event) = 0;
};
