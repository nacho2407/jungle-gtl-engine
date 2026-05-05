#include "GameFramework/CameraShakeBase.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(UCameraShakeBase, UObject)

void UCameraShakeBase::StartShake(
	UCameraManager* Camera,
	float InScale,
	ECameraShakePlaySpace InPlaySpace,
	FRotator InUserPlaySpaceRot)
{
	OwnerCameraManager = Camera;
	Scale = InScale;
	PlaySpace = InPlaySpace;
	UserPlaySpaceRot = InUserPlaySpaceRot;
	bFinished = false;
}

void UCameraShakeBase::UpdateAndApplyCameraShake(float /*DeltaTime*/, FCameraShakeUpdateResult& /*OutResult*/)
{
	// 베이스는 아무것도 하지 않는다. 서브클래스가 override.
}

void UCameraShakeBase::StopShake(bool bImmediately)
{
	// 기본 동작: 즉시 종료. 페이드아웃이 필요한 서브클래스는 override.
	if (bImmediately)
	{
		bFinished = true;
	}
}
