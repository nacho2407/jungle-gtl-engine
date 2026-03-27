#include "BillboardComponent.h"
#include "GameFramework/World.h"
#include "Component/CameraComponent.h"

DEFINE_CLASS(UBillboardComponent, UPrimitiveComponent)

void UBillboardComponent::TickComponent(float DeltaTime)
{
	FVector WorldLocation = GetWorldLocation();

	const UCameraComponent* ActiveCamera = GetOwner()->GetWorld()->GetActiveCamera();

	FVector CameraForward = ActiveCamera->GetForwardVector().Normalized();
	FVector Forward = CameraForward * -1;
	FVector WorldUp = FVector(0.0f, 0.0f, 1.0f);

	if (std::abs(Forward.Dot(WorldUp)) > 0.99f)
	{
		WorldUp = FVector(0.0f, 1.0f, 0.0f); // 임시 Up축 변경
	}

	FVector Right = WorldUp.Cross(Forward).Normalized();
	FVector Up = Forward.Cross(Right).Normalized();

	FMatrix RotMatrix;
	RotMatrix.SetAxes(Forward, Right, Up);

	CachedWorldMatrix = FMatrix::MakeScaleMatrix(GetWorldScale()) * RotMatrix * FMatrix::MakeTranslationMatrix(WorldLocation);

	UpdateWorldAABB();
}