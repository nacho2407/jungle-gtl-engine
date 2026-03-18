#pragma once
#include "CoreTypes.h"
#include "CoreDefine.h"
//#include "FUObjectArray.h"
#include "FUObjectInitializer.h"
#include "UClassData.h"

#include <string>
#include <cstdint>


#define DECLARE_ROOT_UClass(CurrentType)                           \
private:                                                           \
    inline static UClassData* StaticClass = nullptr;               \
public:                                                            \
    static UClassData* GetStaticClass()                            \
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
	virtual UClassData* GetClass() const		                   \
    {                                                              \
        return CurrentType::GetStaticClass();                      \
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
    static UClassData* GetStaticClass()                            \
    {                                                              \
        if (StaticClass != nullptr)                                \
            return StaticClass;                                    \
                                                                   \
        UClassData* ClassData = new UClassData();                  \
        ClassData->ClassName = #CurrentType;                       \
        ClassData->ClassSize = sizeof(CurrentType);                \
        ClassData->SuperClass = SuperType::GetStaticClass();       \
        StaticClass = ClassData;                                   \
        return StaticClass;                                        \
    }															   \
																   \
	virtual UClassData* GetClass() const override                  \
    {                                                              \
        return CurrentType::GetStaticClass();                      \
    }															   \
																   \
	void Initialize(const FUObjectInitializer& ObjectInitilizer)   \
	{															   \
		UUID = ObjectInitilizer.UUID;							   \
		Name = ObjectInitilizer.Name;							   \
	}

class UObject
{
DECLARE_ROOT_UClass(UObject)
private:
	int32 DaechungGarbageCollectionGuanryeon;
	UObject* Outer = nullptr;
protected:
	uint32 UUID;

public:
	UObject() : UUID(0), Name("DefaultObject") {};
	virtual ~UObject();

public:

	uint32 InternalIndex;

	// Owner Object
	UObject* GetOuter() const;
	void SetOuter(UObject* InOuter);


	//template<typename T>
	//CreateDefaultSubobject(FString InName)
	// Unique Object ID
	uint64_t GetID() const;

	// Object Name
	const FString GetName() const;
	void SetName(const FString& InName);

	// Runtime type info
	virtual const char* GetObjClassName() const;

	virtual void Destroy();

	uint32 GetUUID() const { return UUID; }


public:

	uint64_t ObjectID = 0;

	FString Name;
};