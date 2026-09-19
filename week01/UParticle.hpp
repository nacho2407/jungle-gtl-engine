#pragma once

#include "UBall.hpp"

class UParticle : public UBall
{
public:
	FVector Location;
	FVector Velocity;
	float Radius;
public:
	UParticle() :
		Location(0.0f, 0.0f, 0.0f),
		Velocity(0.0f, 0.0f, 0.0f),
		Radius(0.1f)
	{

	}

	~UParticle()
	{

	}

	void SetLocation(const FVector& InLocation) { Location = InLocation; }

	FVector GetLocation() const { return Location; }

	void SetActive(bool bIsActive, float angleRand);

	bool IsActive() { return bActive; }

	void SetAngle(float _Angle)
	{
		Angle = _Angle;
	}

	void SetMaxLifeTime(float time)
	{
		MaxLifeTime = time;
	}

	//TODO 아랫벽에 부딪혔을 때 속도 선형 감소
	void UpdatePhysics(float dt) override;

	void Render(URenderer& renderer) override;

private:
	float EaseOut(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }

private:
	bool bActive = { false };
	float LifeTime = { 0.f };
	float MaxLifeTime = { 0.f };
};
