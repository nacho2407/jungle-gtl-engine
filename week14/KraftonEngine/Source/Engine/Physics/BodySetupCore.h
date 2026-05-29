#pragma once

#include "Object/Object.h"

#include "Source/Engine/Physics/BodySetupCore.generated.h"

UCLASS()
class UBodySetupCore : public UObject
{
public:
	GENERATED_BODY()

protected:
	UPROPERTY(Edit, Save, Category="BodySetup")
	FName BoneName;
};
