#pragma once
#include "Engine/Math/Matrix.h"
#include "Engine/Math/Rotator.h"
#include "Engine/Math/Quat.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Object/Reflection/UStruct.h"

#include "Source/Engine/Math/Transform.generated.h"

USTRUCT()
struct FTransform
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Transform", DisplayName="Location", Type=Vec3, Speed=0.01f)
	FVector Location;

	UPROPERTY(Edit, Save, Category="Transform", DisplayName="Rotation", Type=Vec4, Speed=0.01f)
	FQuat Rotation;

	UPROPERTY(Edit, Save, Category="Transform", DisplayName="Scale", Type=Vec3, Speed=0.01f)
	FVector Scale;

	FTransform() : Location(0.0f, 0.0f, 0.0f), Rotation(FQuat::Identity), Scale(1.0f, 1.0f, 1.0f){}
	FTransform(const FVector& NewLocation, const FQuat& NewRotation, const FVector& NewScale)
		: Location(NewLocation), Rotation(NewRotation), Scale(NewScale) {}

	// FRotator 호환
	FTransform(const FVector& NewLocation, const FRotator& NewRotation, const FVector& NewScale)
		: Location(NewLocation), Rotation(NewRotation.ToQuaternion()), Scale(NewScale) {}

	// FVector 오일러 호환 (X=Roll, Y=Pitch, Z=Yaw)
	FTransform(const FVector& NewLocation, const FVector& EulerRotation, const FVector& NewScale)
		: Location(NewLocation), Rotation(FRotator(EulerRotation).ToQuaternion()), Scale(NewScale) {}

	FTransform(const FVector& InTranslation)
		: Location(InTranslation), Rotation(FQuat::Identity), Scale(1.0f, 1.0f, 1.0f) {}

	FTransform(const FVector& InTranslation, const FRotator& InRotation)
		: Location(InTranslation), Rotation(InRotation.ToQuaternion()), Scale(1.0f, 1.0f, 1.0f) {}

	FTransform(const FMatrix& Mat);
	static FTransform FromMatrixWithScale(const FMatrix& Mat);

	FVector GetLocation() const { return Location; }
	FRotator GetRotator() const { return Rotation.ToRotator(); }
	void SetRotation(const FRotator& Rot) { Rotation = Rot.ToQuaternion(); }
	void SetRotation(const FQuat& Quat) { Rotation = Quat; }

	FVector TransformPosition(const FVector& Position) const;
	FVector TransformVectorNoScale(const FVector& Vector) const;
	FVector TransformPositionNoScale(const FVector& Position) const;
	FVector InverseTransformPositionNoScale(const FVector& Position) const;

	FMatrix ToMatrix() const;

	FTransform operator*(const FTransform& Other) const;
	FTransform& operator*=(const FTransform& Other);
};
