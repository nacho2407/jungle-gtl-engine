#pragma once

#include "Core/CoreTypes.h"
#include "Object/FName.h"
#include "Serialization/Archive.h"

class UAnimNotify;

// AnimSequence 타임라인의 한 지점에서 트리거되는 이벤트.
// Duration > 0 이면 [TriggerTime, TriggerTime+Duration) 구간 동안 활성 (state notify).
// UAnimInstance::TriggerAnimNotifies 가 Previous→Current 구간을 가로지른 notify 들을 모아
//   1) Notify (UAnimNotify*) 가 박혀 있으면 그 객체의 Notify() 호출 (UE 패턴 — 시퀀스 자기 로직 소유)
//   2) 추가로 HandleAnimNotify(가상함수) fallback 후크 호출 (Owner-상태 의존 로직용)
// 두 경로 모두 사용 가능하며, 어느 한 쪽만 사용해도 됨.
struct FAnimNotifyEvent
{
	FName NotifyName;
	float TriggerTime = 0.0f;   // 시퀀스 내 절대 시간 (sec)
	float Duration    = 0.0f;   // 0 이면 instant

	// 로직 객체 포인터. 소유는 UAnimDataModel (Outer 체인). 시퀀스 복사 시 포인터 공유.
	// 직렬화는 raw 필드 (Name/Time/Duration) 만 — Notify 객체의 클래스명/payload 직렬화는
	// UAnimNotify 의 클래스 메타 통합 후 별도 단계로.
	UAnimNotify* Notify = nullptr;

	friend FArchive& operator<<(FArchive& Ar, FAnimNotifyEvent& N)
	{
		Ar << N.NotifyName;
		Ar << N.TriggerTime;
		Ar << N.Duration;
		// Notify 포인터는 미직렬화 — 추후 UAnimNotify 클래스 메타 통합 시 추가.
		return Ar;
	}
};
