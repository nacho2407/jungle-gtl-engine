#include "Animation/AnimNotify.h"

#include "Core/Reflection/ReflectionRegistry.h"
#include "Core/Logging/Log.h"
#include "Object/Class.h"
#include "Object/Object.h"

namespace
{
	TMap<FString, UAnimNotify*> NotifyObjectCache;

	UClass* FindAnimNotifyClass(const FString& NotifyClassName)
	{
		if (NotifyClassName.empty())
		{
			return nullptr;
		}

		UClass* Class = FReflectionRegistry::Get().FindClass(NotifyClassName);
		if (!Class || !Class->IsChildOf(UAnimNotify::StaticClass()) || Class->HasAnyClassFlags(CF_Abstract))
		{
			return nullptr;
		}

		return Class;
	}
}

UAnimNotify* UAnimNotify::GetNotifyObject(const FString& NotifyClassName)
{
	UClass* Class = FindAnimNotifyClass(NotifyClassName);
	if (!Class)
	{
		return nullptr;
	}

	const FString ClassName = Class->GetName();
	auto It = NotifyObjectCache.find(ClassName);
	if (It != NotifyObjectCache.end())
	{
		return It->second;
	}

	UAnimNotify* NotifyObject = Cast<UAnimNotify>(NewObject(Class));
	if (NotifyObject)
	{
		NotifyObjectCache[ClassName] = NotifyObject;
	}
	return NotifyObject;
}

FString UAnimNotify::GetDefaultNotifyClassName(bool bIsState)
{
	return bIsState ? FString("UAnimNotifyState_NamedEvent") : FString("UAnimNotify_NamedEvent");
}

void UAnimNotify_LogEvent::Notify(USkeletalMeshComponent* MeshComponent, const FAnimNotifyStateEvent& Event)
{
	UE_LOG("[AnimNotify_LogEvent] Notify Name=%s Class=%s Time=%.3f Mesh=%p",
		Event.NotifyName.ToString().c_str(),
		Event.NotifyClassName.c_str(),
		Event.TriggerTime,
		MeshComponent);
}

void UAnimNotify_LogEvent::NotifyBegin(USkeletalMeshComponent* MeshComponent, const FAnimNotifyStateEvent& Event)
{
	UE_LOG("[AnimNotify_LogEvent] Begin Name=%s Class=%s Start=%.3f Duration=%.3f Mesh=%p",
		Event.NotifyName.ToString().c_str(),
		Event.NotifyClassName.c_str(),
		Event.TriggerTime,
		Event.Duration,
		MeshComponent);
}

void UAnimNotify_LogEvent::NotifyTick(USkeletalMeshComponent* MeshComponent, const FAnimNotifyStateEvent& Event, float DeltaTime)
{
	UE_LOG("[AnimNotify_LogEvent] Tick Name=%s Class=%s Delta=%.3f Mesh=%p",
		Event.NotifyName.ToString().c_str(),
		Event.NotifyClassName.c_str(),
		DeltaTime,
		MeshComponent);
}

void UAnimNotify_LogEvent::NotifyEnd(USkeletalMeshComponent* MeshComponent, const FAnimNotifyStateEvent& Event)
{
	UE_LOG("[AnimNotify_LogEvent] End Name=%s Class=%s End=%.3f Mesh=%p",
		Event.NotifyName.ToString().c_str(),
		Event.NotifyClassName.c_str(),
		Event.GetEndTime(),
		MeshComponent);
}
