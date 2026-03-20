#include "Object.h"
#include "EngineStatics.h"

TArray<UObject*> GUObjectArray;

UObject::UObject()
{
	// Here for debugging purpose. Remove it later
	UUID = EngineStatics::GenUUID();
	bPendingKill = false;
	InternalIndex = static_cast<uint32>(GUObjectArray.size());
	GUObjectArray.push_back(this);
}

UObject::~UObject()
{
	//uint32 LastIndex = static_cast<uint32>(GUObjectArray.size() - 1);

	//if (InternalIndex != LastIndex)
	//{
	//	UObject* LastObject = GUObjectArray[LastIndex];

	//	GUObjectArray[InternalIndex] = LastObject;

	//	LastObject->InternalIndex = this->InternalIndex;
	//}

	//GUObjectArray.pop_back();

	//EngineStatics::OnDeallocated(sizeof(UObject));
}

const FTypeInfo UObject::s_TypeInfo = { "UObject", nullptr, sizeof(UObject) };

#include "GameFramework/World.h"
void UObjectManager::PurgeScene() {
	for (UObject* Obj : GUObjectArray) {
		if (Obj && Obj->IsA<UWorld>()) {
			UWorld* World = Obj->Cast<UWorld>();
			World->EndPlay();
		}
	}

	CollectGarbage();
	GUObjectArray.erase(
		std::remove(GUObjectArray.begin(), GUObjectArray.end(), nullptr),
		GUObjectArray.end()
	);
}