#pragma once

#include "MovementComponent.h"

class URotatingMovementComponent : public UMovementComponent
{	
public:
	DECLARE_CLASS(URotatingMovementComponent, UMovementComponent)

	virtual void TickComponent(float DeltaTime) override;

	virtual URotatingMovementComponent* Duplicate() override;
	virtual URotatingMovementComponent* DuplicateSubObjects() override { return this; }

	void SetRotationSpeed(const FRotator InRotationRate) { RotationRate = InRotationRate; }
	const FRotator GetRotationRate() const { return RotationRate; }
	
	void SetPivotTranslation(const FVector& InPivot) { PivotTranslation = InPivot; }
    const FVector& GetPivotTranslation() const { return PivotTranslation; }

	void SetRotationInLocalSpace(bool bInLocalSpace) { bRotationInLocalSpace = bInLocalSpace; }
    bool IsRotationInLocalSpace() const { return bRotationInLocalSpace; }

	virtual float GetMaxSpeed() const override { return 0.0f; } // 회전 컴포넌트이므로 0.0f 반환

	virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

private:
	FRotator RotationRate = FRotator::ZeroRotator; 
	FVector PivotTranslation = FVector::ZeroVector; // 회전의 중심점을 표시한다.
	
	bool bRotationInLocalSpace = true;
};