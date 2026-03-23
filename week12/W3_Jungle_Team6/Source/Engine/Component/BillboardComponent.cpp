#include "BillboardComponent.h"

DEFINE_CLASS(UBillboardComponent, UPrimitiveComponent)

void UBillboardComponent::UpdateBillboardMatrix(const FMatrix& ViewMatrix)
{
	FMatrix rotationCancelMatrix = FMatrix::GetCancelRotationMatrix(ViewMatrix);

	CachedWorldMatrix = FMatrix::MakeScaleMatrix(GetRelativeScale()) 
							* rotationCancelMatrix 
							* FMatrix::MakeTranslationMatrix(GetWorldLocation());
}