#include "UParticle.hpp"

void UParticle::SetActive(bool bIsActive, float angleRand) {
	Angle = angleRand;
	Angle += RandRange(-10.f, 10.f);
	float radian = Angle * 3.141592f / 180.f;
	MaxLifeTime = RandRange(0.3f, 0.5f);
	LifeTime = 0.f;
	Velocity.x = cos(radian);
	Velocity.y = sin(radian);

	bActive = bIsActive;
};

void UParticle::UpdatePhysics(float dt)
{
	Velocity = Velocity + dt;

	Location = Location + Velocity * EaseOut(LifeTime / MaxLifeTime) * dt;
	LifeTime += dt;

	if (LifeTime >= MaxLifeTime) bActive = false;
}

void UParticle::Render(URenderer& renderer)
{
	renderer.UpdateConstant(Location, Angle, FVector::Splat(Radius), 1.0f, FVector(1, 1, 1));
	renderer.UpdateTexture(GetTexture());
	renderer.RenderRectangle();
}
