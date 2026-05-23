#pragma once

#include <random>
#include "Core/Types/CoreTypes.h"


struct FRandomStream
{
	std::mt19937 Generator;

	FRandomStream()
		: Generator(std::random_device{}())
	{
	}

	explicit FRandomStream(uint32 Seed)
		: Generator(Seed)
	{
	}

	void Initialize(uint32 Seed)
	{
		Generator.seed(Seed);
	}

	float FRand()
	{
		std::uniform_real_distribution<float> Dist(0.0f, 1.0f);
		return Dist(Generator);
	}

	int32 RandRange(int32 Min, int32 Max)
	{
		std::uniform_int_distribution<int32> Dist(Min, Max);
		return Dist(Generator);
	}
};