#include "Core/CoreMinimal.h"
#include "Engine/EngineStatics.h"
#include "Object.h"

TArray<UObject*> GUObjectArray;

UObject::UObject()
{
	UUID = UEngineStatics::GenUUID();
	InternalIndex = static_cast<uint32>(GUObjectArray.size());
	GUObjectArray.push_back(this);
}

UObject::~UObject()
{
	if (InternalIndex < GUObjectArray.size() && GUObjectArray[InternalIndex] == this)
	{
		GUObjectArray[InternalIndex] = nullptr;
	}
}

void* UObject::operator new(size_t Size)
{
	UEngineStatics::TotalAllocatedBytes += static_cast<uint32>(Size);
	UEngineStatics::TotalAllocationCount++;

	void* Pointer = ::operator new(Size);
	return Pointer;
}

void UObject::operator delete(void* Pointer, size_t Size)
{
	UEngineStatics::TotalAllocatedBytes -= static_cast<uint32>(Size);
	UEngineStatics::TotalAllocationCount--;

	::operator delete(Pointer, Size);
}

REGISTER_CLASS(, UObject)
