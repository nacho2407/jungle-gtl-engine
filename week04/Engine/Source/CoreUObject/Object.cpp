#include "Object.h"
#include "../Engine/EngineStatics.h"
#include "Core/Containers/Array.h"

TArray<UObject*> GUObjectArray;

UObject::UObject()
{
	UUID = UEngineStatics::GenUUID();
	InternalIndex = GUObjectArray.size();
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
	UEngineStatics::TotalAllocatedBytes += Size;
	UEngineStatics::TotalAllocationCount++;

	void* Pointer = ::operator new(Size);
	return Pointer;
}

void UObject::operator delete(void* Pointer, size_t Size)
{
	UEngineStatics::TotalAllocatedBytes -= Size;
	UEngineStatics::TotalAllocationCount--;

	::operator delete(Pointer, Size);
}

REGISTER_CLASS(UObject);
