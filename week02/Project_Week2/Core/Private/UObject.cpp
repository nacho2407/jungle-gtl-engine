#include "UObject.h"

uint64_t UObject::GlobalObjectID = 1;

UObject::UObject()
{
    ObjectID = GlobalObjectID++;
}

UObject::~UObject()
{
}

uint64_t UObject::GetID() const
{
    return ObjectID;
}

const std::string& UObject::GetName() const
{
    return Name;
}

void UObject::SetName(const std::string& InName)
{
    Name = InName;
}

UObject* UObject::GetOuter() const
{
    return Outer;
}

void UObject::SetOuter(UObject* InOuter)
{
    Outer = InOuter;
}

const char* UObject::GetObjClassName() const
{
    return "UObject";
}