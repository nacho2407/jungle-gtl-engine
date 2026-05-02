#include "Core/Random/EngineRandom.h"

#include <algorithm>

FEngineRandom::FEngineRandom()
    : Generator(std::random_device{}())
{
}

void FEngineRandom::SetSeed(uint32 Seed)
{
    Generator.seed(Seed);
}

float FEngineRandom::RandomFloat01()
{
    std::uniform_real_distribution<float> Distribution(0.0f, 1.0f);
    return Distribution(Generator);
}

float FEngineRandom::RandomFloat(float Min, float Max)
{
    if (Min > Max)
    {
        std::swap(Min, Max);
    }

    std::uniform_real_distribution<float> Distribution(Min, Max);
    return Distribution(Generator);
}

int32 FEngineRandom::RandomInt(int32 Min, int32 Max)
{
    if (Min > Max)
    {
        std::swap(Min, Max);
    }

    std::uniform_int_distribution<int32> Distribution(Min, Max);
    return Distribution(Generator);
}

bool FEngineRandom::RandomBool(float Probability)
{
    const float ClampedProbability = std::clamp(Probability, 0.0f, 1.0f);
    std::bernoulli_distribution Distribution(ClampedProbability);
    return Distribution(Generator);
}
