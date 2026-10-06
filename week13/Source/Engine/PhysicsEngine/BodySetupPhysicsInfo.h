#pragma once

#include "Engine/Math/Vector.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Object/Reflection/UStruct.h"

#include "Source/Engine/PhysicsEngine/BodySetupPhysicsInfo.generated.h"

USTRUCT()
struct FBodySetupPhysicsInfo
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Body Physics", DisplayName="Override Mass")
	bool bOverrideMass = false;

	UPROPERTY(Edit, Save, Category="Body Physics", DisplayName="Mass In Kg Override", Min=0.001f, Speed=0.1f)
	float MassInKgOverride = 1.0f;

	UPROPERTY(Edit, Save, Category="Body Physics", DisplayName="Mass Scale", Min=0.001f, Speed=0.05f)
	float MassScale = 1.0f;

	UPROPERTY(Edit, Save, Category="Body Physics", DisplayName="Center Of Mass Offset", Type=Vec3, Speed=0.1f)
	FVector CenterOfMassOffset = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Body Physics", DisplayName="Linear Damping", Min=0.0f, Speed=0.01f)
	float LinearDamping = 0.0f;

	UPROPERTY(Edit, Save, Category="Body Physics", DisplayName="Angular Damping", Min=0.0f, Speed=0.01f)
	float AngularDamping = 0.0f;

	UPROPERTY(Edit, Save, Category="Body Physics", DisplayName="Enable Gravity")
	bool bEnableGravity = true;

	UPROPERTY(Edit, Save, Category="Body Physics", DisplayName="Inertia Tensor Scale", Type=Vec3, Speed=0.01f)
	FVector InertiaTensorScale = FVector(1.0f, 1.0f, 1.0f);
};
