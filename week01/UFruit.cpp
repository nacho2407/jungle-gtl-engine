#include "UFruit.hpp"

void UFruit::SetFruitType(EFruitType InFruitType)
{
	FruitType = InFruitType;
	Radius = SmallestFruitRadius * pow(CoefRadiusGrowth, InFruitType);
	AngularVelocity -= Radius * 2;
	if (AngularVelocity < 0.f)AngularVelocity = 0.f;

	//질량의 지수승인 radius의 영향을 받아서 최소 포도의 질량을 가지도록 설정
	float GrapeRadius = SmallestFruitRadius * pow(CoefRadiusGrowth, 2);

	Mass = max(GrapeRadius * GrapeRadius, Radius * Radius);

	LeftBorder += Radius;
	RightBorder -= Radius;
	TopBorder -= Radius;
	BottomBorder += Radius;
}

void UFruit::UpdatePhysics(float dt)
{
	Location = Location + Velocity * dt;

	if (Location.x < LeftBorder)
	{
		Location.x = LeftBorder;
		Velocity.x *= -1.0f;
	}
	if (Location.x > RightBorder)
	{
		Location.x = RightBorder;
		Velocity.x *= -1.0f;
	}
	if (Location.y <= BottomBorder)
	{
		Location.y = BottomBorder;

		if (fabsf(Velocity.y) < 0.05f)
			Velocity.y = 0.0f;
		else
			Velocity.y *= -RestitutionWall;
	}

	ResolveWallCollision(dt);

	Velocity.y -= GravityAccel * dt;
	const float SleepThreshold = 0.02f;
	if (fabsf(Velocity.x) < SleepThreshold) Velocity.x = 0.0f;
	if (fabsf(Velocity.y) < SleepThreshold) Velocity.y = 0.0f;

	float speed = sqrtf(Velocity.x * Velocity.x + Velocity.y * Velocity.y);

	//각속도를 현재 속도의 크기를 이용하여 조절
	AngularVelocity *= 0.98f;

	const float MaxAngularSpeed = 5.0f;
	if (AngularVelocity > MaxAngularSpeed) AngularVelocity = MaxAngularSpeed;
	else if (AngularVelocity < -MaxAngularSpeed) AngularVelocity = -MaxAngularSpeed;

	Angle += AngularVelocity * dt;
}

bool UFruit::CheckLineCollision(float lineY)
{
	float distance = fabs(Location.y - lineY);
	return distance <= Radius;
}

void UFruit::ResolveWallCollision(float dt)
{
	float normalImpulse = Mass * GravityAccel;
	float frictionImpulse = FloorFriction * normalImpulse;

	if (Location.y <= LeftBorder)
	{
		if (fabsf(Velocity.x) <= frictionImpulse)
			Velocity.x = 0.0f;
		else
			Velocity.x -= (Velocity.x > 0.0f ? 1.0f : -1.0f) * frictionImpulse;
	}
	// 옆벽 마찰 
	if (Location.x <= LeftBorder || Location.x >= RightBorder)
	{
		float frictionImpulse = FloorFriction * normalImpulse;
		if (fabsf(Velocity.y) <= frictionImpulse)
			Velocity.y = 0.0f;
		else
			Velocity.y -= (Velocity.y > 0.0f ? 1.0f : -1.0f) * frictionImpulse;
	}
}

UFruit* CreateFruit(EFruitType FruitType)
{
	UFruit* Fruit = new UFruit();
	Fruit->SetFruitType(FruitType);
	return Fruit;
}

UFruit* CreateFruitAt(EFruitType FruitType, FVector Location, UTexture* InTexture)
{
	UFruit* Fruit = new UFruit();
	Fruit->SetFruitType(FruitType);
	Fruit->SetLocation(Location);
	Fruit->SetTexture(InTexture);

	return Fruit;
}
