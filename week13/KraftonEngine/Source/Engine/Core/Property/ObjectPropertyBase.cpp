#include "ObjectPropertyBase.h"

#include "Object/UClass.h"

UClass* FObjectPropertyBase::GetAllowedClassType() const
{
	return AllowedClass && AllowedClass[0] ? UClass::FindByName(AllowedClass) : nullptr;
}
