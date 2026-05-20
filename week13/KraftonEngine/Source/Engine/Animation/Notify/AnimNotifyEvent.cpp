#include "AnimNotifyEvent.h"

#include "Animation/Notify/AnimNotify.h"
#include "Animation/Notify/AnimNotifyState.h"
#include "Object/Object.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Object/Reflection/UClass.h"
#include "Core/Logging/Log.h"
#include "Serialization/MemoryArchive.h"

namespace
{
	// Notify/NotifyState 같은 UObject pointer 의 직렬화 한 슬롯.
	//   Save: 클래스명 ("None" 이면 null) → payload size(uint32) → SerializeProperties(PF_Save) bytes.
	//   Load: 클래스명 → payload size + bytes 통째로 읽고, 클래스 인스턴스화 성공 시
	//         메모리 아카이브로 properties 복원. 실패해도 메인 스트림 위치는 정확히 다음 슬롯.
	//
	// 과거에는 properties 를 메인 아카이브에 인라인으로 썼는데,
	//   - 클래스가 삭제/리네임되어 ObjectFactory::Create 실패
	//   - 또는 다른 base 로 Cast 실패
	// 이면 SerializeProperties 호출이 통째로 skip → 스트림에 leftover bytes 가 남아
	// 이후 모든 notify (및 그 다음 필드들) 의 read 가 misalign → uasset 전체가 read overrun
	// → "load failed: corrupted package" 로 안 열림. 길이 prefix 가 있으면 슬롯 단위로
	// 정확히 skip 가능해서 깨진 슬롯 1 개만 잃고 시퀀스는 살아남는다.
	// Notify/NotifyState 같은 UObject pointer 의 직렬화 한 슬롯.
	//   Save: 클래스명 ("None" 이면 null) → payload size(uint32) → SerializeProperties(PF_Save) bytes.
	//   Load: 클래스명 → payload size + bytes 통째로 읽고, 클래스 인스턴스화 성공 시
	//         메모리 아카이브로 properties 복원. 실패해도 메인 스트림 위치는 정확히 다음 슬롯.
	//
	// 과거에는 properties 를 메인 아카이브에 인라인으로 썼는데,
	//   - 클래스가 삭제/리네임되어 ObjectFactory::Create 실패
	//   - 또는 다른 base 로 Cast 실패
	// 이면 SerializeProperties 호출이 통째로 skip → 스트림에 leftover bytes 가 남아
	// 이후 모든 notify (및 그 다음 필드들) 의 read 가 misalign → uasset 전체가 read overrun
	// → "load failed: corrupted package" 로 안 열림. 길이 prefix 가 있으면 슬롯 단위로
	// 정확히 skip 가능해서 깨진 슬롯 1 개만 잃고 시퀀스는 살아남는다.
	template<typename T>
	void SerializeNotifyPointer(FArchive& Ar, T*& OutPtr, UObject* InOuter)
	{
		FString ClassName;
		if (Ar.IsSaving())
		{
			ClassName = OutPtr ? FString(OutPtr->GetClass()->GetName()) : FString("None");
		}
		Ar << ClassName;

		if (Ar.IsSaving())
		{
			// 길이 prefix + payload bytes 를 한 번에 쓴다. 메모리 아카이브에 먼저 직렬화 후
			// 사이즈를 알아내야 prefix 를 정확히 채울 수 있다.
			FMemoryArchive PayloadAr(true /*bIsSaving*/);
			if (OutPtr)
			{
				OutPtr->SerializeProperties(PayloadAr, PF_Save);
			}
			const TArray<uint8>& Buffer = PayloadAr.GetBuffer();
			uint32 PayloadSize = static_cast<uint32>(Buffer.size());
			Ar << PayloadSize;
			if (PayloadSize > 0)
			{
				Ar.Serialize(const_cast<uint8*>(Buffer.data()), PayloadSize);
			}
			return;
		}

		// Loading
		uint32 PayloadSize = 0;
		Ar << PayloadSize;
		TArray<uint8> Payload;
		if (PayloadSize > 0)
		{
			Payload.resize(PayloadSize);
			Ar.Serialize(Payload.data(), PayloadSize);
		}

		OutPtr = nullptr;
		if (!ClassName.empty() && ClassName != "None")
		{
			UObject* Created = FObjectFactory::Get().Create(ClassName, InOuter);
			if (Created)
			{
				OutPtr = Cast<T>(Created);
				if (!OutPtr)
				{
					UE_LOG("FAnimNotifyEvent: class '%s' is not a %s — payload discarded (%u bytes).",
					       ClassName.c_str(), T::StaticClass()->GetName(), PayloadSize);
					UObjectManager::Get().DestroyObject(Created);
				}
			}
			else
			{
				UE_LOG("FAnimNotifyEvent: class '%s' not registered in ObjectFactory — payload discarded (%u bytes).",
				       ClassName.c_str(), PayloadSize);
			}
		}

		if (OutPtr && PayloadSize > 0)
		{
			FMemoryArchive PayloadAr(Payload, false /*bIsSaving*/);
			OutPtr->SerializeProperties(PayloadAr, PF_Save);
		}
	}
}

void FAnimNotifyEvent::Serialize(FArchive& Ar, UObject* InOuter)
{
	Ar << NotifyName;
	Ar << TriggerTime;
	Ar << Duration;

	SerializeNotifyPointer<UAnimNotify>(Ar, Notify, InOuter);
	SerializeNotifyPointer<UAnimNotifyState>(Ar, NotifyState, InOuter);
}
