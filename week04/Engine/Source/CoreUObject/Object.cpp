#include "Core/CoreMinimal.h"

#include "Object.h"

#include "CoreUObject/ObjectArray.h"
#include "Engine/EngineStatics.h"

FUObjectArray GUObjectArray;

namespace
{
    TMap<const void*, const char*>& GetAllocatedObjectTypeNames()
    {
        static TMap<const void*, const char*> AllocatedObjectTypeNames;
        return AllocatedObjectTypeNames;
    }

    const char* ResolveAllocatedObjectTypeName(const UObject* Object)
    {
        if (Object == nullptr)
        {
            return "UObject";
        }

        auto& TypeNames = GetAllocatedObjectTypeNames();
        const auto It = TypeNames.find(Object);
        if (It != TypeNames.end() && It->second != nullptr)
        {
            return It->second;
        }

        return "UObject";
    }

    FString ResolveObjectName(const UObject* Object)
    {
        if (Object == nullptr || !Object->Name.IsValid())
        {
            return "<unnamed>";
        }

        return Object->Name.ToFString();
    }
} // namespace

UObject::UObject()
{
	UUID = UEngineStatics::GenUUID();

    GUObjectArray.AllocateObjectIndex(this);

    const FString ObjectName = ResolveObjectName(this);
    UE_LOG(UObject, ELogVerbosity::Log, "Created %s (UUID=%u, Name=%s, Address=%p)",
           ResolveAllocatedObjectTypeName(this), UUID, ObjectName.c_str(), this);
}

UObject::~UObject()
{
    const FString ObjectName = ResolveObjectName(this);
    UE_LOG(UObject, ELogVerbosity::Log, "Destroyed %s (UUID=%u, Name=%s, Address=%p)",
           ResolveAllocatedObjectTypeName(this), UUID, ObjectName.c_str(), this);

	GUObjectArray.FreeObjectIndex(InternalIndex, this);

    GetAllocatedObjectTypeNames().erase(this);
}

void* UObject::operator new(size_t Size)
{
    return AllocateObject(Size, "UObject");
}

void UObject::operator delete(void* Pointer, size_t Size)
{
    FreeObject(Pointer, Size);
}

void* UObject::AllocateObject(size_t Size, const char* InTypeName)
{
	UEngineStatics::TotalAllocatedBytes += static_cast<uint32>(Size);
	UEngineStatics::TotalAllocationCount++;

	void* Pointer = ::operator new(Size);
    GetAllocatedObjectTypeNames()[Pointer] = InTypeName != nullptr ? InTypeName : "UObject";
	return Pointer;
}

void UObject::FreeObject(void* Pointer, size_t Size)
{
    if (Pointer == nullptr)
    {
        return;
    }

	UEngineStatics::TotalAllocatedBytes -= static_cast<uint32>(Size);
	UEngineStatics::TotalAllocationCount--;

	::operator delete(Pointer, Size);
}

REGISTER_CLASS(, UObject)
