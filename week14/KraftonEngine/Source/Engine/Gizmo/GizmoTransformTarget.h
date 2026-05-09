#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Math/Quat.h"
#include "Math/Matrix.h"

class UWorld;

class IGizmoTransformTarget
{
public:
	virtual ~IGizmoTransformTarget() = default;

	virtual bool IsValid() const = 0;
	virtual UWorld* GetWorld() const = 0;

	virtual FVector GetWorldLocation() const = 0;
	virtual FRotator GetWorldRotation() const = 0;
	virtual FVector GetWorldScale() const = 0;

	virtual void SetWorldLocation(const FVector& NewLocation) = 0;
	virtual void SetWorldRotation(const FRotator& NewRotation) = 0;
	virtual void SetWorldScale(const FVector& NewScale) = 0;

	virtual void AddWorldOffset(const FVector& Delta) = 0;
	virtual void AddWorldRotation(const FQuat& Delta, bool bWorldSpace) = 0;
	virtual void AddScaleDelta(const FVector& Delta) = 0;
};
