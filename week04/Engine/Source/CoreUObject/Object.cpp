#include "Object.h"
#include "../Engine/EngineStatics.h"

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

void* UObject::operator new(size_t size)
{
	UEngineStatics::TotalAllocatedBytes += size;
	UEngineStatics::TotalAllocationCount++;

	void* p = ::operator new(size);
	return p;
}

void UObject::operator delete(void* p, size_t size)
{
	UEngineStatics::TotalAllocatedBytes -= size;
	UEngineStatics::TotalAllocationCount--;

	::operator delete(p, size);
}

REGISTER_CLASS(UObject);
