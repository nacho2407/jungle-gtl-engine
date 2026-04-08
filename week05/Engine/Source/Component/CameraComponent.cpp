#include "CameraComponent.h"
#include "Object/Class.h"
#include "Camera/Camera.h"
#include <cmath>

namespace
{
	float ExtractAspectRatio(const UCameraComponent* SourceCameraComponent)
	{
		if (!SourceCameraComponent)
		{
			return 16.0f / 9.0f;
		}

		const FMatrix ProjectionMatrix = SourceCameraComponent->GetProjectionMatrix();
		const float XScale = ProjectionMatrix.M[0][0];
		const float YScale = ProjectionMatrix.M[1][1];
		if (std::fabs(XScale) <= 1e-5f)
		{
			return 16.0f / 9.0f;
		}

		const float AspectRatio = std::fabs(YScale / XScale);
		return AspectRatio > 1e-5f ? AspectRatio : (16.0f / 9.0f);
	}

	void CopyCameraState(const UCameraComponent* SourceCameraComponent, UCameraComponent* DuplicatedCameraComponent)
	{
		if (!SourceCameraComponent || !DuplicatedCameraComponent)
		{
			return;
		}

		const FCamera* SourceCamera = SourceCameraComponent->GetCamera();
		FCamera* DuplicatedCamera = DuplicatedCameraComponent->GetCamera();
		if (!SourceCamera || !DuplicatedCamera)
		{
			return;
		}

		DuplicatedCamera->SetAspectRatio(ExtractAspectRatio(SourceCameraComponent));
		DuplicatedCamera->SetProjectionMode(SourceCamera->GetProjectionMode());
		DuplicatedCamera->SetOrthoWidth(SourceCamera->GetOrthoWidth());
		DuplicatedCamera->SetFOV(SourceCamera->GetFOV());
		DuplicatedCamera->SetSpeed(SourceCamera->GetSpeed());
		DuplicatedCamera->SetMouseSensitivity(SourceCamera->GetMouseSensitivity());
		DuplicatedCamera->SetPosition(SourceCamera->GetPosition());
		DuplicatedCamera->SetRotation(SourceCamera->GetYaw(), SourceCamera->GetPitch());
	}
}

IMPLEMENT_RTTI(UCameraComponent, USceneComponent)

void UCameraComponent::PostConstruct()
{
	bCanEverTick = true;
	Camera = new FCamera();
}

UCameraComponent::~UCameraComponent()
{
	delete Camera;
	Camera = nullptr;
}

void UCameraComponent::Tick(float DeltaTime)
{
	USceneComponent::Tick(DeltaTime);

	//TODO : will be add CameraArm, shake and interpolation  
}

void UCameraComponent::MoveForward(float Value)
{
	Camera->MoveForward(Value);
}

void UCameraComponent::MoveRight(float Value)
{
	Camera->MoveRight(Value);

}

void UCameraComponent::MoveUp(float Value)
{
	Camera->MoveUp(Value);

}

void UCameraComponent::Rotate(float DeltaYaw, float DeltaPitch)
{
	Camera->Rotate(DeltaYaw, DeltaPitch);
}

FCamera* UCameraComponent::GetCamera() const
{
	return Camera;
}

FMatrix UCameraComponent::GetViewMatrix() const
{
	return Camera->GetViewMatrix();
}

FMatrix UCameraComponent::GetProjectionMatrix() const
{
	return Camera->GetProjectionMatrix();
}

void UCameraComponent::DuplicateShallow(UObject* DuplicatedObject, FDuplicateContext& Context) const
{
	USceneComponent::DuplicateShallow(DuplicatedObject, Context);
	CopyCameraState(this, static_cast<UCameraComponent*>(DuplicatedObject));
}

void UCameraComponent::SetFov(float inFov)
{
	Camera->SetFOV(inFov);
}

void UCameraComponent::SetSpeed(float Inspeed)
{
	Camera->SetSpeed(Inspeed);
}

void UCameraComponent::SetSensitivity(float InSetSensitivity)
{
	Camera->SetMouseSensitivity(InSetSensitivity);
}
