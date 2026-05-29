#pragma once

#include "Object/Object.h"
#include "ConstraintInstance.h"

struct FPhysicsConstraintProfileHandle
{
	FConstraintProfileProperties ProfileProperties;
	FName ProfileName;
};

class UPhysicsConstraintTemplate : public UObject
{
public:
	UPhysicsConstraintTemplate() = default;
	~UPhysicsConstraintTemplate() override = default;

	FConstraintInstance DefaultInstance;

	const TArray<FPhysicsConstraintProfileHandle>& GetProfileHandles() const
	{
		return ProfileHandles;
	}

	TArray<FPhysicsConstraintProfileHandle>& GetProfileHandlesMutable()
	{
		return ProfileHandles;
	}

	bool ContainsConstraintProfile(FName ProfileName) const;
	void AddConstraintProfile(FName ProfileName);
	void RemoveConstraintProfile(FName ProfileName);
	void ApplyConstraintProfile(FName ProfileName, FConstraintInstance& ConstraintInstance, bool bDefaultIfNotFound) const;

private:
	TArray<FPhysicsConstraintProfileHandle> ProfileHandles;
};
