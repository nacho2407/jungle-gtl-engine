#include "BillboardComponent.h"
#include <cmath>
#include "GameFramework/World.h"
#include "Editor/Viewport/ViewportCamera.h"

DEFINE_CLASS(UBillboardComponent, UPrimitiveComponent)

bool UBillboardComponent::TryGetActiveCamera(const FViewportCamera*& OutCamera) const
{
	OutCamera = nullptr;

	if (GetOwner() == nullptr || GetOwner()->GetWorld() == nullptr)
	{
		return false;
	}

	OutCamera = GetOwner()->GetWorld()->GetActiveCamera();
	return OutCamera != nullptr;
}
// 카메라 Forward, Right, Up Vector 기반으로 billboard 의 world 행렬 생성 
FMatrix UBillboardComponent::MakeBillboardWorldMatrix(
	const FVector& WorldLocation,
	const FVector& WorldScale,
	const FVector& CameraForward,
	const FVector& CameraRight,
	const FVector& CameraUp)
{
	FVector Forward = CameraForward.GetSafeNormal();
	FVector Right = (-CameraRight).GetSafeNormal();
	FVector Up = CameraUp.GetSafeNormal();

	if (Forward.IsNearlyZero())
	{
		Forward = FVector(-1.0f, 0.0f, 0.0f);
	}

	if (Right.IsNearlyZero() || Up.IsNearlyZero())
	{
		FVector FallbackUp = FVector::UpVector;
		if (std::abs(FVector::DotProduct(Forward, FallbackUp)) > 0.99f)
		{
			FallbackUp = FVector::RightVector;
		}

		Right = FVector::CrossProduct(FallbackUp, Forward).GetSafeNormal();
		Up = FVector::CrossProduct(Forward, Right).GetSafeNormal();
	}

	FMatrix BillboardMatrix = FMatrix::Identity;
	BillboardMatrix.SetAxes(
		Forward * WorldScale.X,
		Right * WorldScale.Y,
		Up * WorldScale.Z,
		WorldLocation);
	return BillboardMatrix;
}

void UBillboardComponent::TickComponent(float DeltaTime)
{
	(void)DeltaTime;
	UpdateWorldAABB();
}
