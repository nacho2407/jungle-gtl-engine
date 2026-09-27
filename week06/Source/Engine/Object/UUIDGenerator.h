#pragma once
#include "Core/CoreTypes.h"

class UUIDGenerator
{
public:
	static uint32 GenUUID()
	{
		return NextUUID++;
	}

	static void ResetUUIDGeneration(int Value) { NextUUID = Value; }

private:
	static uint32 NextUUID;
};
