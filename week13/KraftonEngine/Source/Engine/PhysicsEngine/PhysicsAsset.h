#pragma once

#include "BodySetup.h"
#include "Object/GarbageCollection.h"

class UPhysicsAsset : public UObject
{
public:
	const TArray<UBodySetup*>& GetBodySetups() const { return BodySetups; }
	TArray<UBodySetup*>& GetBodySetupsMutable() { return BodySetups; }

	void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		UObject::AddReferencedObjects(Collector);
		Collector.AddReferencedObjects(BodySetups, "UPhysicsAsset.BodySetups");
	}

private:
	TArray<UBodySetup*> BodySetups;
};
