#pragma once
#include "UObject.h"
#include "FVector.h"
#include "FMatrix.h"

class USceneComponent : public UObject
{
	DECLARE_RTTI(USceneComponent, UObject)
public:
	USceneComponent();
	virtual ~USceneComponent() override;

public:
	FVector RelativeLocation;
	FVector RelativeRotation;
	FVector RelativeScale3D;

	FMatrix GetRelativeMatrix() const;
};
