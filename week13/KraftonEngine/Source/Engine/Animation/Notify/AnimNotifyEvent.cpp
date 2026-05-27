#include "AnimNotifyEvent.h"

#include "Animation/Notify/AnimNotify.h"
#include "Animation/Notify/AnimNotifyState.h"
#include "Object/Object.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Object/Reflection/UClass.h"
#include "Core/Logging/Log.h"
#include "Serialization/MemoryArchive.h"
#include "Object/GarbageCollection.h"

namespace
{
	// 진입부 마커 — 호환 reader 가 신/구 포맷을 구분하는 데 사용.
	// 값은 NotifyName 의 FString length 로 절대 나올 수 없는 sentinel.
	// (FName 직렬화는 [Length:uint32][bytes]. 4GB 짜리 이름은 없으므로 0xFADE0001 은 안전.)
	static constexpr uint32 GAnimNotifyEventFormatMagic   = 0xFADE0001u;
	static constexpr uint32 GAnimNotifyEventFormatVersion = 1u;

	// Notify/NotifyState 같은 UObject pointer 의 한 슬롯 (신포맷, v1+):
	//   ClassName(FString) → PayloadSize(uint32) → properties bytes.
	// 길이 prefix 로 인해 클래스 인스턴스화 실패해도 슬롯 단위로 skip 가능 → 깨진 슬롯 1 개만 잃고
	// 시퀀스는 살아남음.
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

	// Legacy (pre-da696c0d): ClassName 다음에 properties 가 메인 아카이브에 그대로 인라인.
	// 클래스 인스턴스화 실패 시 properties 를 skip 할 방법이 없어 스트림 misalign — 해당 슬롯
	// 이후가 모두 깨짐. 호환 reader 라 그 위험은 그대로 안고 간다 (옛 자산을 한 번 로드 후
	// 재저장하면 신포맷으로 마이그레이션됨).
	template<typename T>
	void SerializeNotifyPointerLegacy(FArchive& Ar, T*& OutPtr, UObject* InOuter)
	{
		FString ClassName;
		Ar << ClassName;

		OutPtr = nullptr;
		if (!ClassName.empty() && ClassName != "None")
		{
			UObject* Created = FObjectFactory::Get().Create(ClassName, InOuter);
			if (Created)
			{
				OutPtr = Cast<T>(Created);
				if (!OutPtr)
				{
					UObjectManager::Get().DestroyObject(Created);
					UE_LOG("FAnimNotifyEvent legacy: class '%s' is not a %s — stream may corrupt after this slot.",
					       ClassName.c_str(), T::StaticClass()->GetName());
				}
			}
			else
			{
				UE_LOG("FAnimNotifyEvent legacy: class '%s' not registered — stream may corrupt after this slot.",
				       ClassName.c_str());
			}
		}

		if (OutPtr)
		{
			OutPtr->SerializeProperties(Ar, PF_Save);
		}
	}
}

void FAnimNotifyEvent::Serialize(FArchive& Ar, UObject* InOuter)
{
	if (Ar.IsSaving())
	{
		// 신포맷: magic + version 마커 → 표준 entry.
		uint32 Magic   = GAnimNotifyEventFormatMagic;
		uint32 Version = GAnimNotifyEventFormatVersion;
		Ar << Magic;
		Ar << Version;

		Ar << NotifyName;
		Ar << TriggerTime;
		Ar << Duration;

		SerializeNotifyPointer<UAnimNotify>(Ar, Notify, InOuter);
		SerializeNotifyPointer<UAnimNotifyState>(Ar, NotifyState, InOuter);
		return;
	}

	// Loading — 첫 uint32 가 magic 이면 신포맷, 아니면 옛 포맷의 NotifyName 길이.
	uint32 FirstU32 = 0;
	Ar << FirstU32;

	if (FirstU32 == GAnimNotifyEventFormatMagic)
	{
		uint32 Version = 0;
		Ar << Version;
		(void)Version; // 현재 v1 만 — 향후 분기 추가 시 사용.

		Ar << NotifyName;
		Ar << TriggerTime;
		Ar << Duration;

		SerializeNotifyPointer<UAnimNotify>(Ar, Notify, InOuter);
		SerializeNotifyPointer<UAnimNotifyState>(Ar, NotifyState, InOuter);
		return;
	}

	// Legacy 경로: FirstU32 는 NotifyName 의 FString 길이였음. 그 길이만큼 데이터를 직접
	// 읽어 NotifyName 재구성, 이후 필드는 옛 layout 으로 진행.
	FString NotifyNameStr;
	NotifyNameStr.resize(FirstU32);
	if (FirstU32 > 0)
	{
		Ar.Serialize(NotifyNameStr.data(), FirstU32 * sizeof(char));
	}
	NotifyName = FName(NotifyNameStr);

	Ar << TriggerTime;
	Ar << Duration;

	SerializeNotifyPointerLegacy<UAnimNotify>(Ar, Notify, InOuter);
	SerializeNotifyPointerLegacy<UAnimNotifyState>(Ar, NotifyState, InOuter);
}

void FAnimNotifyEvent::AddReferencedObjects(FReferenceCollector& Collector) const
{
	Collector.AddReferencedObject(Notify);
	Collector.AddReferencedObject(NotifyState);
}
