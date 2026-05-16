#pragma once
#include "PrimitiveComponent.h"

UCLASS()
class UShapeComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UShapeComponent, UPrimitiveComponent)

	void PostDuplicate(UObject* Original) override;

private:
	UPROPERTY(DisplayName = "Shape Color")
	FColor ShapeColor;

	UPROPERTY(DisplayName = "Draw Only If Selected")
	bool bDrawOnlyIfSelected;

	// UPrimitiveComponent을(를) 통해 상속됨
	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
	EPrimitiveType GetPrimitiveType() const override;
};
