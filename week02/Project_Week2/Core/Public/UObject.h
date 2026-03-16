#pragma once
#include "CoreGlobal.h"
#include "CoreTypes.h"
#include "CoreDefine.h"
#include "FUObjectArray.h"
#include "FUObjectInitializer.h"
#include "UObjectBaseUtility.h"
#include "UClassData.h"

#include <string>
#include <cstdint>


#define CreateMetaData(CurrentType)                           \
private:                                                           \
    inline static UClassData* StaticClass = nullptr;               \
public:                                                            \
    static UClassData* GetClass()                                  \
    {                                                              \
        if (StaticClass != nullptr)                                \
            return StaticClass;                                    \
                                                                   \
        UClassData* classData = new UClassData();                  \
        classData->ClassName = #CurrentType;                       \
        classData->ClassSize = sizeof(CurrentType);                \
        StaticClass = classData;                                   \
        return StaticClass;                                        \
	}

class UObject : public UObjectBaseUtility
{
	CreateMetaData(UObject)

public:
	size_t AllocatedSize;
private:
	uint32 UUID;

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

	uint32 GetUUID() const { return UUID; }

public:

	uint64_t ObjectID = 0; // @@@ ???

	FString Name;

	UObject* Outer = nullptr; /// 이거는 Private가 맞는듯?

};