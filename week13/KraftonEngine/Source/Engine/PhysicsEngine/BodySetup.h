#pragma once
#include "BodySetupCore.h"
#include "AggregateGeom.h"

class UBodySetup : public UBodySetupCore
{
public:
	const FKAggregateGeom& GetAggGeom() const { return AggGeom; }
	FKAggregateGeom& GetAggGeom() { return AggGeom; }

private:
	// DisplayName = Primitives
	FKAggregateGeom AggGeom;
};
