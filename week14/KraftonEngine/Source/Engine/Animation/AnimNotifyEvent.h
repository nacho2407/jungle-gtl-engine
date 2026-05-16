#pragma once

#include "Core/CoreTypes.h"
#include "Object/FName.h"

// AnimSequence 타임라인의 한 지점에서 트리거되는 이벤트.
// Duration > 0 이면 [TriggerTime, TriggerTime+Duration) 구간 동안 활성 (state notify).
// B 의 UAnimInstance::TriggerAnimNotifies 가 Previous→Current 구간을 가로지른
// notify 들을 모아 HandleAnimNotify(가상함수) 로 디스패치한다.
struct FAnimNotifyEvent
{
	FName NotifyName;
	float TriggerTime = 0.0f;   // 시퀀스 내 절대 시간 (sec)
	float Duration    = 0.0f;   // 0 이면 instant
};
