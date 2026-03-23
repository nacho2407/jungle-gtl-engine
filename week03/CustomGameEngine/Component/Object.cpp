#include "Object.h"
#include "EngineCore/EngineStatics.h"

TArray<UObject*> GUObjectArray;

UObject::UObject()
{
	UUID = UEngineStatics::GenUUID();
	InternalIndex = GUObjectArray.size();
	GUObjectArray.push_back(this);
}

UObject::~UObject()
{
	if (InternalIndex < GUObjectArray.size() && GUObjectArray[InternalIndex] == this)
	{
		GUObjectArray[InternalIndex] = nullptr;
	}
	FUniqueNameGenerator::ReleaseName(Name);
}


REGISTER_CLASS(UObject);
