#include "Component/CameraComponent.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerCameraManager.h"
#include "Render/Types/MinimalViewInfo.h"
#include <cmath>

IMPLEMENT_CLASS_WITH_PROPERTIES(UCameraComponent, USceneComponent)

BEGIN_PROPERTY_REGISTRATION(UCameraComponent)
	EDIT_PROPERTY_RANGE(UCameraComponent, CameraState.FOV, "FOV", EPropertyType::Float, "Camera", 0.1f, 3.14f, 0.01f)
	EDIT_PROPERTY_RANGE(UCameraComponent, CameraState.NearZ, "Near Z", EPropertyType::Float, "Camera", 0.01f, 100.0f, 0.01f)
	EDIT_PROPERTY_RANGE(UCameraComponent, CameraState.FarZ, "Far Z", EPropertyType::Float, "Camera", 1.0f, 100000.0f, 10.0f)
	EDIT_PROPERTY(UCameraComponent, CameraState.bIsOrthogonal, "Orthographic", EPropertyType::Bool, "Camera")
	EDIT_PROPERTY_RANGE(UCameraComponent, CameraState.OrthoWidth, "Ortho Width", EPropertyType::Float, "Camera", 0.1f, 1000.0f, 0.5f)
END_PROPERTY_REGISTRATION()

void UCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	// E.2/3: PC 가 BeginPlay 시점엔 아직 spawn 전 → PlayerCameraManager nullptr.
	// PC 의 BeginPlay 에서 World 의 모든 카메라 컴포넌트를 catch up 등록하므로 안전.
	if (UWorld* World = GetOwner()->GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (APlayerCameraManager* CM = PC->GetPlayerCameraManager())
			{
				CM->RegisterCamera(this);
			}
		}
	}
}

void UCameraComponent::EndPlay()
{
	Super::EndPlay();
	if (UWorld* World = GetOwner()->GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (APlayerCameraManager* CM = PC->GetPlayerCameraManager())
			{
				CM->UnregisterCamera(this);
			}
		}
	}
}

void UCameraComponent::LookAt(const FVector& Target)
{
	FVector Position = GetWorldLocation();
	FVector Diff = (Target - Position).Normalized();

	constexpr float Rad2Deg = 180.0f / 3.14159265358979f;

	FRotator LookRotation = GetRelativeRotation();
	LookRotation.Pitch = -asinf(Diff.Z) * Rad2Deg;

	if (fabsf(Diff.Z) < 0.999f) {
		LookRotation.Yaw = atan2f(Diff.Y, Diff.X) * Rad2Deg;
	}

	SetRelativeRotation(LookRotation);
}

void UCameraComponent::OnResize(int32 Width, int32 Height)
{
	CameraState.AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
}

void UCameraComponent::SetCameraState(const FCameraState& NewState)
{
	CameraState = NewState;
}

void UCameraComponent::GetCameraView(float /*DeltaTime*/, FMinimalViewInfo& OutPOV) const
{
	UpdateWorldMatrix();
	OutPOV.Location    = GetWorldLocation();
	OutPOV.Rotation    = GetWorldMatrix().ToRotator();
	OutPOV.FOV         = CameraState.FOV;
	OutPOV.AspectRatio = CameraState.AspectRatio;
	OutPOV.OrthoWidth  = CameraState.OrthoWidth;
	OutPOV.NearClip    = CameraState.NearZ;
	OutPOV.FarClip     = CameraState.FarZ;
	OutPOV.bIsOrtho    = CameraState.bIsOrthogonal;
}

void UCameraComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);
}
