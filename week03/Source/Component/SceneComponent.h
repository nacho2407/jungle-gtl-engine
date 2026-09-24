#pragma once

#include "Math/Matrix.h"
#include "Math/Quaternion.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Object.h"

#include "ThirdParty/json.h"

class UScene;

class USceneComponent : public UObject
{
	DECLARE_RTTI(USceneComponent, UObject)
public:
	USceneComponent();
	virtual ~USceneComponent() override;

public:
	UScene* Scene = nullptr;

	inline const FVector GetRelativeLocation() const { return RelativeLocation; }
	inline const FRotator GetRelativeRotation() const { return RelativeRotation; }
	inline const FVector GetRelativeScale3D() const { return RelativeScale3D; }
	inline const FQuat GetRelativeQuaternion() const { return RelativeQuaternion; }
	inline virtual void SetRelativeLocation(const FVector& newLocation) { RelativeLocation = newLocation; }
	inline virtual void SetRelativeRotation(const FRotator& newRotation) { RelativeRotation = newRotation; }
	inline virtual void SetRelativeScale3D(const FVector& newScale) { RelativeScale3D = newScale; }
	inline virtual void SetRelativeQuaternion(FQuat newQuat) { RelativeQuaternion = newQuat; RelativeRotation = newQuat.ToEuler(); }

	bool bIsSelected = false;

	FMatrix GetRelativeMatrix();

	virtual void OnComponentAdded() {};
	virtual void Update(float deltaTime) {};
	virtual json::JSON Serialize();
	virtual void Deserialize(json::JSON);
	virtual void DrawDebugLines() {}

	bool IsSelected() const;

private:
	FVector RelativeLocation;
	FRotator RelativeRotation;
	FVector RelativeScale3D;
	FQuat RelativeQuaternion;
};
