#include "BillboardComponent.h"
#include <cmath>
#include "GameFramework/World.h"
#include "Editor/Viewport/ViewportCamera.h"

DEFINE_CLASS(UBillboardComponent, UPrimitiveComponent)

// UpdateWorldAABB 등의 함수를 오버라이드하지 않았기 때문에 UBillboradComponent도 추상 클래스가 됩니다.
// 추후에 UBillboardComponent를 사용할 일이 있다면 Duplicate의 주석을 해제하고 수정하시면 됩니다.

// 객체를 동적 생성한 뒤, 부모 클래스의 프로퍼티부터 내려오며 깊은 복사합니다.
//UBillboardComponent* UBillboardComponent::Duplicate()
//{
//    UBillboardComponent* NewComp = UObjectManager::Get().CreateObject<UBillboardComponent>();
//
//	  NewComp->SetActive(this->IsActive());
//    NewComp->SetOwner(nullptr);
//    
//    NewComp->SetRelativeLocation(this->GetRelativeLocation());
//    NewComp->SetRelativeRotation(this->GetRelativeRotation());
//    NewComp->SetRelativeScale(this->GetRelativeScale());
//    
//    NewComp->SetVisibility(this->IsVisible());
//
//    NewComp->bIsBillboard = this->bIsBillboard;
//
//    return NewComp;
//}

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
