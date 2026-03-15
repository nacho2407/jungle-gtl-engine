#pragma once
#include "CoreTypes.h"

class UObject
{
    size_t AllocatedSize;
    uint32 InternalIndex;
    uint32 UUID;
protected:


public:
    FString Name;

    virtual ~UObject() = default;

    virtual void Init(uint32 InUUID, FString InName);

    void* operator new(size_t size);

    void operator delete(void* ptr, size_t size) noexcept;

    template<typename T>
    void CreateDefaultSubobject()
    {

    }

    uint32 GetUUID() const
    {
        return UUID;
    }
};

inline TArray<UObject*> GUObjectArray;