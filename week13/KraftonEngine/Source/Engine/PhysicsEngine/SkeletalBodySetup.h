#pragma once

#include "BodySetup.h"

class USkeletalBodySetup : public UBodySetup
{
public:
	USkeletalBodySetup() = default;
	~USkeletalBodySetup() override = default;

	bool bSkipScaleFromAnimation = false;
};
