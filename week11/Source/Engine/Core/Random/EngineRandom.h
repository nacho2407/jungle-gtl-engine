#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"

#include <random>

class FEngineRandom : public TSingleton<FEngineRandom>
{
    friend class TSingleton<FEngineRandom>;

public:
    void SetSeed(uint32 Seed);
    float RandomFloat01();
    float RandomFloat(float Min, float Max);
    int32 RandomInt(int32 Min, int32 Max);
    bool RandomBool(float Probability);

private:
    FEngineRandom();

private:
    std::mt19937 Generator;
};
