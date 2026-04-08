#include "ActorComponent.h"
#include "Object/Class.h"
#include "Serializer/Archive.h"

IMPLEMENT_RTTI(UActorComponent, UObject)

void UActorComponent::DuplicateShallow(UObject* DuplicatedObject, FDuplicateContext& Context) const
{
	UActorComponent* DuplicatedComponent = static_cast<UActorComponent*>(DuplicatedObject);
	DuplicatedComponent->Owner = nullptr;
	DuplicatedComponent->bRegistered = false;
	DuplicatedComponent->bBegunPlay = false;
	DuplicatedComponent->bCanEverTick = bCanEverTick;
	DuplicatedComponent->bTickEnabled = bTickEnabled;
}

void UActorComponent::FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const
{
	UActorComponent* DuplicatedComponent = static_cast<UActorComponent*>(DuplicatedObject);
	DuplicatedComponent->Owner = Context.FindDuplicate(Owner.Get());
}

void UActorComponent::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving()) Ar.Serialize("UUID", UUID);
	else
	{
		if (Ar.Contains("UUID"))
		{
			uint32 SavedUUID = 0;
			Ar.Serialize("UUID", SavedUUID);

			GUUIDToObjectMap.erase(UUID);

			if (auto It = GUUIDToObjectMap.find(SavedUUID); It != GUUIDToObjectMap.end() && It->second != this)
			{
				It->second->UUID = 0;
				GUUIDToObjectMap.erase(It);
			}
			UUID = SavedUUID;
			GUUIDToObjectMap[SavedUUID] = this;
		}
	}
}
