//#pragma once
//
//#include "CoreTypes.h"
//#include "UObject.h"
//#include "Actor/AActor.h"
//
//class FActorInstanceManager
//{
//private:
//	THashMap<FString, AActor*> NameToInstance;
//
//	AActor* Instantiate(AActor* Target);
//public:
//	FActorInstanceManager();
//
//	const void RegisterInstance(AActor* InData);
//	const UClassData* GetClassData(FString InName);
//	AActor* GetActorInstance(FString InName);
//};
//
//inline FActorInstanceManager GUActorInstanceManager;