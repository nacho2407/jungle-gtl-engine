#include "MovementComponent.h"

class ProjectileMovementComponent : public UMovementComponent
{
	virtual void TickComponent(float DeltaTime) override;

	void SetRotationSpeed(FRotator InRotationRate) { RotationRate = InRotationRate; }
	const FRotator GetRotationSpeed() const { return RotationRate; }

private:
	FRotator RotationRate = FRotator(0.0f, 0.0f, 0.0f); 
	FVector PivotTranslation = FVector(0.0f, 0.0f, 0.0f); // 회전의 중심점을 표시한다.
	
	bool bRotationInLocalSpace = true;
};