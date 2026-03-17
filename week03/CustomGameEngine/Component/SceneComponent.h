#pragma once

#include "Math/Matrix.h"
#include "Math/Quaternion.h"
#include "Math/Vector.h"
#include "Object.h"
#include "Serializable.h"

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
	inline const FQuat GetRelativeQuaternion() { return RelativeQuaternion; }
	inline virtual void SetRelativeLocation(FVector newLocation) { RelativeLocation = newLocation; }
	inline virtual void SetRelativeRotation(FVector newRotation) { RelativeRotation = newRotation; }
	inline virtual void SetRelativeScale3D(FVector newScale) { RelativeScale3D = newScale; }
	inline virtual void SetRelativeQuaternion(FQuat newQuat) { RelativeQuaternion = newQuat; RelativeRotation = newQuat.ToEuler(); }

	bool bIsSelected = false;

	FMatrix GetRelativeMatrix();

	virtual void Update(float deltaTime) {};
	virtual json::JSON Serialize();
	virtual void Deserialize(json::JSON);

	virtual void DrawProperties();

	bool IsSelected() const;

private:
	FVector RelativeLocation;
	FVector RelativeRotation;
	FVector RelativeScale3D;
	FQuat RelativeQuaternion;
};
