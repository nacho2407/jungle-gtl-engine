#include "UEngineStatics.h"
#include "UObject.h"

void UObject::Init(uint32 InUUID, FString InName)
{
    UUID = InUUID;
    Name = InName;
}

void* UObject::operator new(std::size_t size)
{
    auto NewObject = (UObject*)::operator new(size);

    uint32 ObjectSize = static_cast<uint32>(size);
    NewObject->AllocatedSize = ObjectSize;

    UEngineStatics::TotalAllocationBytes += ObjectSize;
    UEngineStatics::TotalAllocationCount += 1;
    NewObject->InternalIndex = static_cast<uint32>(GUObjectArray.size());
    GUObjectArray.push_back(NewObject);

    return NewObject;
}

void UObject::operator delete(void* ptr, size_t size) noexcept
{
    auto DeleteObject = (UObject*)ptr;
    
    std::size_t AllocationSize = DeleteObject->AllocatedSize;

    UEngineStatics::TotalAllocationBytes -= static_cast<uint32>(size);
    UEngineStatics::TotalAllocationCount--;

    auto TargetIndex = DeleteObject->InternalIndex;


    auto temp = GUObjectArray[TargetIndex];
    GUObjectArray[TargetIndex] = GUObjectArray.back();
    GUObjectArray[TargetIndex]->InternalIndex = TargetIndex;
    GUObjectArray.pop_back();

    ::operator delete(ptr);
}