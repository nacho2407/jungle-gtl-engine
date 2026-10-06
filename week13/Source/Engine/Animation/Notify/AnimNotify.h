#pragma once

#include "Object/Object.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;

// UE 의 UAnimNotify 모방.
//   - 시퀀스 타임라인의 instant notify 로직을 담는 베이스 클래스.
//   - 시퀀스(UAnimDataModel) 가 자기 Notify 객체를 소유 — UE 패턴.
//   - 자식이 Notify() 오버라이드. UAnimInstance::TriggerAnimNotifies 가 dispatch.
//   - UCLASS/GENERATED_BODY 로 런타임 클래스 정보를 등록한다.
//
// 라이프타임: 보통 UAnimDataModel 을 Outer 로 두어 mock/import 시 생성된 객체가
// 데이터 모델과 같이 살아간다. UObjectManager 가 실제 소멸을 관리.

#include "Source/Engine/Animation/Notify/AnimNotify.generated.h"

UCLASS()
class UAnimNotify : public UObject
{
public:
	GENERATED_BODY()
	UAnimNotify() = default;
	~UAnimNotify() override = default;

	// dispatch entry. MeshComp 는 현재 재생 중인 컴포넌트, Anim 은 해당 시퀀스.
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim)
	{
		(void)MeshComp; (void)Anim;
	}
};
