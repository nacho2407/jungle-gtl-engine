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
	inline const FVector GetRelativeLocation() { return RelativeLocation; }
	inline const FVector GetRelativeRotation() { return RelativeRotation; }
	inline const FVector GetRelativeScale3D() { return RelativeScale3D; }
	inline virtual void SetRelativeLocation(FVector newLocation) { RelativeLocation = newLocation; }
	inline virtual void SetRelativeRotation(FVector newRotation) { RelativeRotation = newRotation; }
	inline virtual void SetRelativeScale3D(FVector newScale) { RelativeScale3D = newScale; }

	bool bIsSelected = false;

	FMatrix GetRelativeMatrix() const;

	virtual void Update(float deltaTime) {};
	virtual json::JSON Serialize();
	virtual void Deserialize(json::JSON);

	virtual void DrawProperties();

	bool IsSelected() const;

private:
	FVector RelativeLocation;
	FVector RelativeRotation;
	FVector RelativeScale3D;
};
