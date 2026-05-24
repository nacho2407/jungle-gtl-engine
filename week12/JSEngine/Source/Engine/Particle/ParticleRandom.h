#pragma once

#include "Core/CoreMinimal.h"

/**
 * @brief 비교적 무거운 std::mt19937 같은 고급 난수 생성기 대신, 간단한 선형 합동 생성기(LCG)를 사용하여
 *        particle 시스템에서 가볍고 빠르게 사용할 수 있도록 합니다
 * 
 * @note 실제 Unreal Engine의 소스 코드(Math/RandomStream.h) 또한 가벼운 LCG를 사용하여 구현되어 있습니다.
 *       단, LCG는 하위 비트에서 패턴이 강하게 나타날 수 있다는 단점이 있으므로, 모듈로 연산을 최대한 사용하지 않도록 주의하여 구현합니다
 */
struct FParticleRandomStream
{
	uint32 InitialSeed = 1u;
	uint32 State = 1u;

	void Initialize(uint32 InSeed)
	{
		InitialSeed = InSeed != 0u ? InSeed : 1u;
		State = InitialSeed;
	}

	void Reset()
	{
		State = InitialSeed;
	}

	uint32 GetUnsignedInt()
	{
		State = State * 1664525u + 1013904223u;
		return State;
	}

	float GetFraction()
	{
		return static_cast<float>(GetUnsignedInt() >> 8) * (1.0f / 16777216.0f);
	}

	float GetRange(float Min, float Max)
	{
		return Min + (Max - Min) * GetFraction();
	}

	int32 GetRange(int32 Min, int32 Max)
    {
        if (Max <= Min)
        {
            return Min;
        }

        const int32 Range = (Max - Min) + 1;

		// GetUnsignedInt() % Range로 구현할 수도 있으나, 하위 비트에서 패턴이 강하게 나타날 수 있는 LCG의 특성을 고려하여,
        // GetFraction()을 활용하여 범위 내의 정수를 반환하도록 구현(이것두 Unreal Engine 스타일)
        return Min + static_cast<int32>(GetFraction() * static_cast<float>(Range));
    }
};
