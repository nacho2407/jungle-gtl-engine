#pragma once

#include "Object/Object.h"
//#include "ConstraintInstance.h"

//struct FConstraintProfileProperties
//{
//	//FLinearConstraint LinearLimit;
//	//FConeConstraint ConeLimit;
//	//FTwistConstraint TwistLimit;
//
//	bool bDisableCollision = true;
//	bool bParentDominates = false;
//	bool bEnableProjection = true;
//
//	float ProjectionLinearTolerance = 5.0f;
//	float ProjectionAngularTolerance = 180.0f;
//
//	float ProjectionLinearAlpha = 1.0f;
//	float ProjectionAngularAlpha = 1.0f;
//
//	bool bLinearBreakable = false;
//	bool bAngularBreakable = false;
//
//	float LinearBreakThreshold = 0.0f;
//	float AngularBreakThreshold = 0.0f;
//};

//struct FPhysicsConstraintProfileHandle
//{
//	FConstraintProfileProperties ProfileProperties;
//	FName ProfileName;
//};

class UPhysicsConstraintTemplate : public UObject
{
public:
	UPhysicsConstraintTemplate() = default;
	~UPhysicsConstraintTemplate() override = default;

//	FConstraintInstance DefaultInstance;
//
//	const TArray<FPhysicsConstraintProfileHandle>& GetProfileHandles() const
//	{
//		return ProfileHandles;
//	}
//
//	TArray<FPhysicsConstraintProfileHandle>& GetProfileHandlesMutable()
//	{
//		return ProfileHandles;
//	}
//
//	bool ContainsConstraintProfile(FName ProfileName) const;
//	void AddConstraintProfile(FName ProfileName);
//	void RemoveConstraintProfile(FName ProfileName);
//	void ApplyConstraintProfile(FName ProfileName, FConstraintInstance& ConstraintInstance, bool bDefaultIfNotFound) const;
//
//private:
//	TArray<FPhysicsConstraintProfileHandle> ProfileHandles;
};
