#pragma once
#include "Core.h"

class AActor : public UObject
{
public:
	AActor() : UObject() {}
	AActor(const FUObjectInitializer& ObjectInitilizer) : UObject(ObjectInitilizer) {}


	//USceneComponent* RootCompoent = nullptr;
	//FVector GetActorLocation()
	//{
	//	return RootCompoent->GetLocation();
	//}

	//template <typename T = UActorComponent >
	//T* CreateDefaultSubobject(FString FName)
	//{
	//	return nullptr;
	//}

	//virtual void Tick(float DeltaTime);
};