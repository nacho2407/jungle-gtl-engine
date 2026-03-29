#include "BillboardComponent.h"
#include "GameFramework/World.h"
#include "Editor/Viewport/ViewportCamera.h"

DEFINE_CLASS(UBillboardComponent, UPrimitiveComponent)

void UBillboardComponent::TickComponent(float DeltaTime)
{
	FVector WorldLocation = GetWorldLocation();

	if (GetOwner() == nullptr || GetOwner()->GetWorld() == nullptr)
	{
		return;
	}

	const FViewportCamera* ActiveCamera = GetOwner()->GetWorld()->GetActiveCamera();
	if (ActiveCamera == nullptr)
	{
		return;
	}

	FVector CameraForward = ActiveCamera->GetForwardVector().GetSafeNormal();
	FVector Forward = CameraForward * -1;
	FVector WorldUp = FVector(0.0f, 0.0f, 1.0f);

	if (std::abs(FVector::DotProduct(Forward,WorldUp)) > 0.99f)
	{
		WorldUp = FVector(0.0f, 1.0f, 0.0f); // 임시 Up축 변경
	}

	FVector Right = FVector::CrossProduct(WorldUp,Forward).GetSafeNormal();
	FVector Up = FVector::CrossProduct(Forward,Right).GetSafeNormal();

	FMatrix RotMatrix;
	RotMatrix.SetAxes(Forward, Right, Up);

	CachedWorldMatrix = FMatrix::MakeScaleMatrix(GetWorldScale()) * RotMatrix * FMatrix::MakeTranslationMatrix(WorldLocation);

	UpdateWorldAABB();
}