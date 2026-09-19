#pragma once

#include "EFruitType.h"

#include "UBall.hpp"

class UFruit : public UBall
{
public:
	UFruit()
	{
		FruitType = EFruitType::FT_Cherry;
	}

	EFruitType GetFruitType() const { return FruitType; }

	void SetFruitType(EFruitType InFruitType);

	void UpdatePhysics(float dt) override;

	bool CheckLineCollision(float lineY);

private:
	/**
	* 옆과 아래 벽과의 마찰
	*/
	void ResolveWallCollision(float dt);

private:
	const float SmallestFruitRadius = 0.06f; // 가장 작은 과일의 반지름
	const float CoefRadiusGrowth = 1.25f; // 과일 레벨이 하나씩 올라갈 때마다 반지름이 커지는 정도

	EFruitType FruitType;
};

// RAII 고려 필요 (자원 해제가 지금은 제대로 안됨)
// 스마트포인터 사용 필요
UFruit* CreateFruit(EFruitType FruitType);

UFruit* CreateFruitAt(EFruitType FruitType, FVector Location, UTexture* InTexture = nullptr);
