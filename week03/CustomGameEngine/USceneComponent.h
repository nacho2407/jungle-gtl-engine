#pragma once
#include "UObject.h"
#include "FVector.h"
#include "FMatrix.h"
#include "ISerializable.h"

class USceneComponent : public UObject, public ISerializable
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

	virtual json::JSON Serialize();
	virtual void Deserialize(json::JSON);
};
