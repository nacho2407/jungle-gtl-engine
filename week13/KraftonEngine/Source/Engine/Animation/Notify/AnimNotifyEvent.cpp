#include "AnimNotifyEvent.h"

#include "Animation/Notify/AnimNotify.h"
#include "Animation/Notify/AnimNotifyState.h"
#include "Object/Object.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Object/Reflection/UClass.h"
#include "Core/Logging/Log.h"

namespace
{
	// Notify/NotifyState 같은 UObject pointer 의 직렬화 한 슬롯.
	//   Save: 클래스명 ("None" 이면 null) → SerializeProperties(PF_Save).
	//   Load: 클래스명 읽어 ObjectFactory::Create(Outer) → SerializeProperties(PF_Save).
	// T 는 UAnimNotify / UAnimNotifyState 둘 다 가능하지만 형식 안전을 위해 호출부에서 Cast.
	template<typename T>
	void SerializeNotifyPointer(FArchive& Ar, T*& OutPtr, UObject* InOuter)
	{
		FString ClassName;
		if (Ar.IsSaving())
		{
			ClassName = OutPtr ? FString(OutPtr->GetClass()->GetName()) : FString("None");
		}
		Ar << ClassName;

		if (Ar.IsLoading())
		{
			OutPtr = nullptr;
			if (!ClassName.empty() && ClassName != "None")
			{
				UObject* Created = FObjectFactory::Get().Create(ClassName, InOuter);
				if (Created)
				{
					OutPtr = Cast<T>(Created);
					if (!OutPtr)
					{
						UE_LOG("FAnimNotifyEvent: class '%s' is not a %s — ignored.",
						       ClassName.c_str(), T::StaticClass()->GetName());
					}
				}
				else
				{
					UE_LOG("FAnimNotifyEvent: class '%s' not registered in ObjectFactory.",
					       ClassName.c_str());
				}
			}
		}

		// payload (UPROPERTY(Save)) — save/load 어느 쪽이든 객체가 있어야 호출.
		if (OutPtr)
		{
			OutPtr->SerializeProperties(Ar, PF_Save);
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
