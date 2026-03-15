#include "UEngineStatics.h"
#include "UObject.h"

UObject::UObject(const FUObjectInitializer& ObjectInitilizer) : UObject()
{
	UUID = ObjectInitilizer.UUID;
	Name = ObjectInitilizer.Name;
}

//void* UObject::operator new(std::size_t size)
//{
//    auto NewObject = (UObject*)::operator new(size);
//
//    uint32 ObjectSize = static_cast<uint32>(size);
//    NewObject->AllocatedSize = ObjectSize;
//
//    int32 index = GUObjectArray.GetAvailableIndex();
//    int32 SerialNumber = GUObjectArray.AllocateSerialNumber(index);
//    GUObjectArray.AllocatdUObjectIndex(NewObject, SerialNumber);    
//    
//    FUObjectItem* ObjectItem = GUObjectArray.ObjectToObjectItem(NewObject);
//    ObjectItem->ObjectSize = ObjectSize;
//
//    UEngineStatics::TotalAllocationBytes += ObjectSize;
//    UEngineStatics::TotalAllocationCount += 1;
//
//    return NewObject;
//}
//
//void UObject::operator delete(void* ptr, size_t size) noexcept
//{
//    auto DeleteObject = (UObject*)ptr;
//    
//    std::size_t AllocationSize = DeleteObject->AllocatedSize;
//
//    UEngineStatics::TotalAllocationBytes -= static_cast<uint32>(size);
//    UEngineStatics::TotalAllocationCount--;
//
//    FUObjectItem* ObjectItem = GUObjectArray.ObjectToObjectItem(DeleteObject);
//
//
//    auto TargetIndex = DeleteObject->InternalIndex;
//    GUObjectArray.FreeUObjectIndox(DeleteObject);
//
//    //auto temp = GUObjectArray[TargetIndex];
//    //GUObjectArray[TargetIndex] = GUObjectArray.back();
//    //GUObjectArray[TargetIndex]->InternalIndex = TargetIndex;
//    //GUObjectArray.pop_back();
//
//    ::operator delete(ptr);
//}