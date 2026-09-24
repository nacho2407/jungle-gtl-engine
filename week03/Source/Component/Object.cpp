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
		while (!GUObjectArray.empty() && GUObjectArray.back() == nullptr)
		{
			GUObjectArray.pop_back();
		}
		if (GUObjectArray.empty())
		{
			GUObjectArray.shrink_to_fit();
		}
	}
	FUniqueNameGenerator::ReleaseName(Name);
}

void UObject::SetName(const FString& InName)
{
	if (Name.GetPlainDisplayName() == InName)
	{
		return;
	}

	// 기존 번호 해제함
	FUniqueNameGenerator::ReleaseName(Name);
	// 새 이름으로 고유 번호 다시 할당함
	Name = FUniqueNameGenerator::MakeUniqueName(InName);
}

REGISTER_CLASS(UObject);
