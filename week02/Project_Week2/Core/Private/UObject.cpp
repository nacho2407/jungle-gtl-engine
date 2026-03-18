#include "UEngineStatics.h"
#include "UObject.h"
#include "FMemory.h"
#include "FUObjectFactory.h"
//void UObject::Initialize(const FUObjectInitializer& ObjectInitilizer)
//{
//	UUID = ObjectInitilizer.UUID;
//	Name = ObjectInitilizer.Name;
//}

//void UObject::Initialize(const FUObjectInitializer& ObjectInitilizer)
//{
//}

UObject::~UObject()
{
}

UObject* UObject::GetOuter() const
{
    return Outer;
}

void UObject::SetOuter(UObject* InOuter)
{
    Outer = InOuter;
}

uint64_t UObject::GetID() const
{
    return ObjectID;
}

const FString UObject::GetName() const
{
    return Name;
}

void UObject::SetName(const FString& InName)
{
    Name = InName;
}


const char* UObject::GetObjClassName() const
{
    return "UObject";
}

void UObject::Destroy()
{
    DestroyObject(this);
}

