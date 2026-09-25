#include "CoreUObject/UObjectArray.h"

#include <utility>

void FUObjectArray::AllocateObjectIndex(UObject* Object)
{
    if (Object == nullptr)
    {
        UE_LOG(FUObjectArray, ELogVerbosity::Warning, "null object was entered");

        return;
    }

	FUObjectItem ObjectItem{ Object };
    uint32      Index = Objects.size();

    if (!FreeIndices.empty())
    {
        Index = FreeIndices.back();
        FreeIndices.pop_back();
        Object->InternalIndex = Index;
        Objects[Index] = std::move(ObjectItem);
    }
    else
    {
        Object->InternalIndex = Index;
        Objects.push_back(std::move(ObjectItem));
    }
}

void FUObjectArray::FreeObjectIndex(uint32 Index, UObject* Object)
{
    if (Index >= Objects.size() || Objects[Index].Object == nullptr || Objects[Index].Object != Object)
    {
        UE_LOG(FUObjectArray, ELogVerbosity::Error, "unable to perform the free operation due to an invalid value being entered");
        return;
    }

    Objects[Index].Object = nullptr;
    FreeIndices.push_back(Index);
}

const FUObjectItem* FUObjectArray::GetObjectItem(uint32 Index) const
{
    if (Index >= Objects.size())
    {
        UE_LOG(FUObjectArray, ELogVerbosity::Error, "out of range");
        return nullptr;
    }

    return &Objects[Index];
}
