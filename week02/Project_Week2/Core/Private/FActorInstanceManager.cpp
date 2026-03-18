//#include "FActorInstanceManager.h"
//
//AActor* FActorInstanceManager::Instantiate(AActor* Target)
//{
//	return nullptr;
//}
//
//FActorInstanceManager::FActorInstanceManager()
//{
//	//여기서 메타데이터 삽입
//}
//
//const void FActorInstanceManager::RegisterInstance(AActor* InActor)
//{
//	UClassData* ClassData = InActor->GetClass();
//
//	FString ClassName = ClassData->ClassName;
//	if (NameToInstance.contains(ClassName))
//		return;
//
//	NameToInstance[ClassName] = InActor;
//}
//
//const UClassData* FActorInstanceManager::GetClassData(FString InName)
//{
//	return NameToInstance[InName]->GetClass();
//}
//
//AActor* FActorInstanceManager::GetActorInstance(FString InName)
//{
//	return NameToInstance[InName];
//}
