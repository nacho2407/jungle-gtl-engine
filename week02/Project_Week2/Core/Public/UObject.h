#pragma once
#include "CoreTypes.h"
#include "FUObjectArray.h"
#include "FUObjectInitializer.h"
#include "UObjectBaseUtility.h"

class UObject : public UObjectBaseUtility
{
    size_t AllocatedSize;
    uint32 UUID;

protected:


public:
    FString Name;

    UObject() : AllocatedSize(0), UUID(0), Name("DefaultObject") {};
    UObject(const FUObjectInitializer& ObjectInitilizer);

    //virtual void Init() = 0;

    //void* operator new(size_t size);

    //void operator delete(void* ptr, size_t size) noexcept;

    //template<typename T>
    //void CreateDefaultSubobject()
    //{

    //}

    uint32 GetUUID() const
    {
        return UUID;
    }
};

//inline TArray<UObject*> GUObjectArray;