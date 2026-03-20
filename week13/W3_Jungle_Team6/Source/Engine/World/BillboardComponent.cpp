#include "BillboardComponent.h"


void UBillboardComponent::UpdateBillboardMatrix(const FMatrix& ViewMatrix)
{
	FMatrix rotationCancelMatrix = FMatrix::GetCancelRotationMatrix(ViewMatrix);

	CachedWorldMatrix = FMatrix::MakeScaleMatrix(GetRelativeScale()) 
							* rotationCancelMatrix 
							* FMatrix::MakeTranslationMatrix(GetWorldLocation());
}

bool UBillboardComponent::GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand)
{
	UpdateBillboardMatrix(viewMatrix);

	OutCommand.Type = ERenderCommandType::Billboard;
	OutCommand.TransformConstants.Model = CachedWorldMatrix;

	return true;
}