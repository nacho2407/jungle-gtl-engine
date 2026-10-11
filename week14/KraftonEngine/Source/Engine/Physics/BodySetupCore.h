#pragma once

#include "Object/Object.h"

#include "Source/Engine/Physics/BodySetupCore.generated.h"

UCLASS()
class UBodySetupCore : public UObject
{
public:
	GENERATED_BODY()
	
	FName GetBoneName() const {return BoneName;}
	void SetBoneName(FName InBoneName) {BoneName = InBoneName;}

protected:
	UPROPERTY(Edit, Save, Category="BodySetup")
	FName BoneName;
};
