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


#define DECLARE_ROOT_UClass(CurrentType)                           \
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
		classData->SuperClass = nullptr;						   \
        StaticClass = classData;                                   \
        return StaticClass;                                        \
	}															   \
																   \
	void Initialize(const FUObjectInitializer& ObjectInitilizer)   \
	{															   \
		UUID = ObjectInitilizer.UUID;							   \
		Name = ObjectInitilizer.Name;							   \
	}

#define DECLARE_UClass(CurrentType, SuperType)                     \
private:                                                           \
    inline static UClassData* StaticClass = nullptr;               \
public:                                                            \
    static UClassData* GetClass()                                  \
    {                                                              \
        if (StaticClass != nullptr)                                \
            return StaticClass;                                    \
                                                                   \
        UClassData* ClassData = new UClassData();                  \
        ClassData->ClassName = #CurrentType;                       \
        ClassData->ClassSize = sizeof(CurrentType);                \
        ClassData->SuperClass = SuperType::GetClass();             \
        StaticClass = ClassData;                                   \
        return StaticClass;                                        \
    }															   \
																   \
	void Initialize(const FUObjectInitializer& ObjectInitilizer)   \
	{															   \
		UUID = ObjectInitilizer.UUID;							   \
		Name = ObjectInitilizer.Name;							   \
	}

class UObject : public UObjectBaseUtility
{
DECLARE_ROOT_UClass(UObject)

public:
	size_t AllocatedSize;
protected:
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