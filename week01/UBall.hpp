#pragma once

#include "UPrimitive.hpp"

class UBall : public UPrimitive
{
public:
	FVector Location; // 여기서는 Offset과 동일한 개념
	FVector Velocity;

	float Angle;
	float AngularVelocity;

	float Radius;

	// 무게 비례 공 밀어내기에 사용(ResolveCollision 참조)
	float Mass;

public:
	UBall(UTexture* InTexture = nullptr) :
		UPrimitive(InTexture),
		Location(0.0f, 0.0f, 0.0f),
		Velocity(0.0f, 0.0f, 0.0f),
		Radius(0.1f),
		Mass(0.0f),
		Angle(0.f),
		AngularVelocity(1.f)
	{
		Mass = Radius * Radius * MassFactor;

		LeftBorder += Radius * 0.2f;
		RightBorder -= Radius * 0.2f;
		TopBorder -= Radius * 0.2f;
		BottomBorder += Radius * 0.2f;

		Location.x = RandRange(LeftBorder, RightBorder);
		Location.y = 1.f;
		Location.z = 0.0f;

		Velocity.x = 0.f;
		Velocity.y = -1.f;
		Velocity.z = 0.0f;
	}

	~UBall()
	{

	}

	void SetLocation(const FVector& InLocation) { Location = InLocation; }

	FVector GetLocation() const { return Location; }

	void Render(URenderer& renderer) override;

	void BorderClamp();

protected:
	float MassFactor = 1.0f; // 질량 보정용

	const float GravityAccel = 2.f;    // 중력 가속도
	const float RestitutionWall = 0.1f;
	const float FloorFriction = 0.6f;

protected:
	float Random() { return (float)rand() / (float)RAND_MAX; } // [0, 1]

	float RandRange(float a, float b) { return a + (b - a) * Random(); }
};
