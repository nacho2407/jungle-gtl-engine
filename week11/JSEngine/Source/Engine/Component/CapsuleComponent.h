#pragma once
#include "ShapeComponent.h"

#include "CapsuleComponent.generated.h"

UCLASS()
class UCapsuleComponent : public UShapeComponent
{
    GENERATED_BODY_UCapsuleComponent()
public:
    DECLARE_CLASS(UCapsuleComponent, UShapeComponent)
    float GetCapsuleHalfHeight() const { return CapsuleHalfHeight; }
    float GetCapsuleRadius() const { return CapsuleRadius; }

	void UpdateWorldAABB() const override;

    float GetScaledCapsuleHalfHeight() const 
	{
        FVector Scale = GetWorldScale();
        return CapsuleHalfHeight * std::abs(Scale.Z);
	}
    
	float GetScaledCapsuleRadius() const
    {
        FVector Scale = GetWorldScale();
        return CapsuleRadius * std::abs(Scale.Z);
    }

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostDuplicate(UObject* Original) override;
    void Serialize(FArchive& Ar) override;

private:
    UPROPERTY(EditAnywhere)
    float CapsuleHalfHeight = 0.5f;
    UPROPERTY(EditAnywhere)
    float CapsuleRadius = 0.5f;

    bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
    EPrimitiveType GetPrimitiveType() const override;
};
