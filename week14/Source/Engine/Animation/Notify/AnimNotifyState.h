#pragma once

#include "Object/Object.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;
class FArchive;

// UE 의 UAnimNotifyState 모방. Duration > 0 인 구간 notify 의 로직 객체.
//   - UAnimNotify (instant) 는 Notify() 1 회만 호출.
//   - UAnimNotifyState 는 [TriggerTime, TriggerTime+Duration) 구간 동안:
//        프레임 Begin → 매 프레임 Tick(frameDelta) → 끝나는 프레임 End.
//   - AnimInstance::ActiveNotifyStates 가 active 인스턴스 추적 → 다음 프레임 안 보이면 End 자동 발사.
//
// 활용 예: 발자국 사운드 (Tick 에 거리체크 + PlaySound), 공격 hit 윈도우 (Begin → 활성, End → 비활성),
//          무기 trail (Begin/End 로 trail emitter on/off).
//
// 라이프타임: UAnimDataModel 을 Outer 로 두며 UObjectManager 가 소멸 관리 (UAnimNotify 와 동일).
// 직렬화: 클래스명 + UPROPERTY(Save) payload — FAnimNotifyEvent::Serialize(Ar, Outer) 에서 처리.

#include "Source/Engine/Animation/Notify/AnimNotifyState.generated.h"

UCLASS()
class UAnimNotifyState : public UObject
{
public:
	GENERATED_BODY()
	UAnimNotifyState() = default;
	~UAnimNotifyState() override = default;

	// 구간 진입 시 1 회. TotalDuration 은 본 이벤트의 전체 지속(sec) — Begin 시점에 외부 시스템이
	// alloc 할 buffer 사이즈 결정 등에 사용 가능.
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim, float TotalDuration)
	{
		(void)MeshComp; (void)Anim; (void)TotalDuration;
	}

	// 매 프레임 — FrameDeltaTime 은 본 이벤트의 활성 구간과 현재 프레임 [Prev, Cur) 의 교집합 폭.
	// (보통 owner 의 DeltaSeconds 와 같지만, 이벤트 시작/끝 프레임은 더 작을 수 있음.)
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim, float FrameDeltaTime)
	{
		(void)MeshComp; (void)Anim; (void)FrameDeltaTime;
	}

	// 구간 종료 시 1 회. 자연 종료 / 시퀀스 전환 / weight drop 등 모두 호출됨.
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim)
	{
		(void)MeshComp; (void)Anim;
	}
};
