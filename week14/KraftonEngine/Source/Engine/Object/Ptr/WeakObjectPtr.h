#pragma once
#include "Core/Types/CoreTypes.h"
#include "Object/Object.h"

class FWeakObjectPtr
{
public:
	FWeakObjectPtr() = default;
	FWeakObjectPtr(UObject* InObject)
	{
		if (IsValid(InObject))
		{
			ObjectIndex = InObject->GetInternalIndex();
			SerialNumber = InObject->GetSerialNumber();
		}
	}

	UObject* Get() const
	{
		if (ObjectIndex == UINT32_MAX)
		{
			return nullptr;
		}

		if (ObjectIndex >= GUObjectSlots.size())
		{
			return nullptr;
		}

		const FObjectSlot& Slot = GUObjectSlots[ObjectIndex];
		if (Slot.SerialNumber != SerialNumber)
		{
			return nullptr;
		}

		UObject* Object = Slot.Object;
		if (!Object)
		{
			return nullptr;
		}

		if (Object->GetInternalIndex() != ObjectIndex || Object->GetSerialNumber() != SerialNumber)
		{
			return nullptr;
		}

		if (Object->IsPendingKill())
		{
			return nullptr;
		}

		return Object;
	}

	bool IsValid() const
	{
		return Get() != nullptr;
	}

private:
	uint32 ObjectIndex = UINT32_MAX;
	uint32 SerialNumber = 0;
};
