#pragma once
#include <extensions/PxD6Joint.h>

#include "Math/Transform.h"
#include "Object/FName.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Object/Reflection/UStruct.h"

#include "Source/Engine/Physics/ConstraintInstance.generated.h"

struct FBodyInstance;

USTRUCT()
struct FConstraintInstanceInitDesc
{
	GENERATED_BODY()

	FBodyInstance* ParentBody = nullptr;
	FBodyInstance* ChildBody = nullptr;

	UPROPERTY(VisibleAnywhere, Category="Constraint", DisplayName="Parent Bone")
	FName ParentBoneName = FName::None;

	UPROPERTY(VisibleAnywhere, Category="Constraint", DisplayName="Child Bone")
	FName ChildBoneName = FName::None;

	FTransform ParentFrame;
	FTransform ChildFrame;

	UPROPERTY(Edit, Category="Constraint", DisplayName="ParentFrame Location", Member=ParentFrame.Location, Type=Vec3, Speed=0.1f);
	UPROPERTY(Edit, Category="Constraint", DisplayName="ChildFrame Location", Member=ChildFrame.Location, Type=Vec3, Speed=0.1f);
	UPROPERTY(Edit, Category="Constraint", DisplayName="Twist Limit", Min=0.0f, Max=180.0f, Speed=0.25f)
	float TwistLimitDegrees = 45.0f;

	UPROPERTY(Edit, Category="Constraint", DisplayName="Swing1 Limit", Min=0.0f, Max=180.0f, Speed=0.25f)
	float Swing1LimitDegrees = 30.0f;

	UPROPERTY(Edit, Category="Constraint", DisplayName="Swing2 Limit", Min=0.0f, Max=180.0f, Speed=0.25f)
	float Swing2LimitDegrees = 30.0f;

	UPROPERTY(Edit, Category="Constraint", DisplayName="Enable Collision")
	bool bEnableCollision = false;

	UPROPERTY(Edit, Category = "Constraint|Projection", DisplayName = "Enable Projection")
	bool bEnableProjection = true;

	UPROPERTY(Edit, Category = "Constraint|Projection", DisplayName = "Projection Linear Tolerance", Min = 0.0f, Speed = 0.1f)
	float ProjectionLinearTolerance = 10.0f;

	UPROPERTY(Edit, Category = "Constraint|Projection", DisplayName = "Projection Angular Tolerance", Min = 0.0f, Max = 180.0f, Speed = 0.25f)
	float ProjectionAngularToleranceDegrees = 30.0f;
};

struct FConstraintInstance
{
	FBodyInstance* ParentBody = nullptr;
	FBodyInstance* ChildBody = nullptr;

	FName ParentBoneName = FName::None;
	FName ChildBoneName = FName::None;

	FTransform ParentFrame;
	FTransform ChildFrame;

	float TwistLimitDegrees = 45.0f;
	float Swing1LimitDegrees = 30.0f;
	float Swing2LimitDegrees = 30.0f;

	float ProjectionLinearTolerance = 10.0f;
	float ProjectionAngularToleranceDegrees = 30.0f;

	bool bEnableProjection = true;
	bool bEnableCollision = false;

	physx::PxD6Joint* Joint = nullptr;

	bool IsValidConstraintInstance() const { return Joint != nullptr; }
	void ClearPhysicsPointers() { Joint = nullptr; }

	float GetTwistLimitRadians() const;
	float GetSwing1LimitRadians() const;
	float GetSwing2LimitRadians() const;
};

