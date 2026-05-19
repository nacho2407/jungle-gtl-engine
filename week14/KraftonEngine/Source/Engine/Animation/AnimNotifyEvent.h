#pragma once

#include "Core/CoreTypes.h"
#include "Object/FName.h"
#include "Serialization/Archive.h"

class UAnimNotify;
class UAnimNotifyState;
class UObject;

// AnimSequence 타임라인의 한 지점에서 트리거되는 이벤트.
// Duration > 0 + NotifyState != null 이면 [TriggerTime, TriggerTime+Duration) 구간 동안 활성
// state notify (Begin/Tick/End). Duration == 0 (or NotifyState 미연결) 이면 instant.
//
// 두 로직 객체를 동시에 박을 수 있으나 보통 하나만 사용:
//   - Notify (UAnimNotify*)         : instant 트리거 (1 회 호출)
//   - NotifyState (UAnimNotifyState*): 구간 활성 (Begin/Tick/End)
// 둘 다 없으면 fallback 으로 AnimInstance::HandleAnimNotify(NotifyName) 만 dispatch.
//
// 라이프타임: 두 로직 객체 모두 UAnimDataModel 을 Outer 로 두고 UObjectManager 가 소멸 관리.
// 시퀀스 복사 시 포인터 공유 (얕은 복사 — UE 와 동일).
struct FAnimNotifyEvent
{
	FName NotifyName;
	float TriggerTime = 0.0f;   // 시퀀스 내 절대 시간 (sec)
	float Duration    = 0.0f;   // 0 이면 instant (NotifyState 동작 안 함)

	UAnimNotify*      Notify      = nullptr;
	UAnimNotifyState* NotifyState = nullptr;

	// raw 필드만 직렬화 (Notify/NotifyState 포인터는 무시).
	// 풀 직렬화는 FAnimNotifyEvent::Serialize(Ar, Outer) — Outer 가 있어야 ObjectFactory 로
	// 인스턴스 복원 가능하므로 별도 메서드. UAnimDataModel::Serialize 가 매 entry 마다 호출.
	friend FArchive& operator<<(FArchive& Ar, FAnimNotifyEvent& N)
	{
		Ar << N.NotifyName;
		Ar << N.TriggerTime;
		Ar << N.Duration;
		return Ar;
	}

	// Outer 인지 직렬화 — Notify/NotifyState 객체의 클래스명 + UPROPERTY(Save) payload 까지.
	// 로드 시 ObjectFactory::Create 로 클래스 이름으로 인스턴스 생성, Outer 로 InOuter 설정.
	// InOuter 는 보통 UAnimDataModel — Notify 객체의 라이프타임은 DataModel 과 함께.
	void Serialize(FArchive& Ar, UObject* InOuter);
};
