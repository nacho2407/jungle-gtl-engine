#pragma once
#include "CoreTypes.h"
#include "FUObjectArray.h"
#include "FUObjectInitializer.h"
#include "UObjectBaseUtility.h"

#include <string>
#include <cstdint>

class UObject : public UObjectBaseUtility
{
    size_t AllocatedSize;
    uint32 UUID;

protected:


public:

    UObject() : AllocatedSize(0), UUID(0), Name("DefaultObject") {};
    UObject(const FUObjectInitializer& ObjectInitilizer);
    virtual ~UObject();

public:

    // Unique Object ID
    uint64_t GetID() const;

    // Object Name
    const std::string& GetName() const;
    void SetName(const std::string& InName);

    // Owner Object
    UObject* GetOuter() const;
    void SetOuter(UObject* InOuter);

    // Runtime type info
    virtual const char* GetObjClassName() const;

protected:

    uint64_t ObjectID = 0;

    FString Name;

    UObject* Outer = nullptr;

private:

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