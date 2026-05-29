#pragma once

#include "BodySetup.h"

class UPhysicsAsset : public UObject
{
	TArray<UBodySetup*> BodySetup;
};
