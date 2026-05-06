#include "GameFramework/CameraShakeCameraModifier.h"
#include "GameFramework/CameraShakeBase.h"
#include "GameFramework/PlayerCameraManager.h"
#include "Math/Quat.h"
#include "Object/ObjectFactory.h"
#include "Object/UClass.h"
#include "Render/Types/MinimalViewInfo.h"
#include <algorithm>

namespace
{
	// PlaySpace 별 location offset 변환:
	//   World       — raw 가산
	//   CameraLocal — 현재 POV 회전 기준 회전
	//   UserDefined — UserPlaySpaceRot 기준 회전
	FVector ConvertShakeLocationToWorld(
		const FVector& Location,
		const FRotator& CameraRotation,
		ECameraShakePlaySpace PlaySpace,
		const FRotator& UserPlaySpaceRot)
	{
		switch (PlaySpace)
		{
		case ECameraShakePlaySpace::CameraLocal:
			return CameraRotation.ToQuaternion().RotateVector(Location);
		case ECameraShakePlaySpace::UserDefined:
			return UserPlaySpaceRot.ToQuaternion().RotateVector(Location);
		case ECameraShakePlaySpace::World:
		default:
			return Location;
		}
	}
}

IMPLEMENT_CLASS(UCameraModifier_CameraShake, UCameraModifier)

UCameraModifier_CameraShake::UCameraModifier_CameraShake()
{
	// shake 는 비교적 늦게 적용 (다른 효과 위에 흔들림이 얹어지는 게 자연). priority 100.
	Priority = 100.0f;
}

bool UCameraModifier_CameraShake::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	// 베이스가 alpha 페이드 갱신 후 exclusive=false 반환.
	UCameraModifier::ModifyCamera(DeltaTime, InOutPOV);

	// 각 active shake 의 결과를 PlaySpace 기준으로 World offset 으로 변환 후 합산.
	FCameraShakeUpdateResult Sum;
	for (UCameraShakeBase* Shake : ActiveShakes)
	{
		if (!Shake || Shake->IsFinished()) continue;

		FCameraShakeUpdateResult Per;
		Shake->UpdateAndApplyCameraShake(DeltaTime, Per);

		Sum.Location += ConvertShakeLocationToWorld(
			Per.Location,
			InOutPOV.Rotation,
			Shake->GetPlaySpace(),
			Shake->GetUserPlaySpaceRot());
		Sum.Rotation.Pitch += Per.Rotation.Pitch;
		Sum.Rotation.Yaw   += Per.Rotation.Yaw;
		Sum.Rotation.Roll  += Per.Rotation.Roll;
		Sum.FOV            += Per.FOV;
	}

	// 종료된 셰이크 정리 — UpdateAndApplyCameraShake 안에서 IsFinished 토글된 것 포함.
	ActiveShakes.erase(
		std::remove_if(ActiveShakes.begin(), ActiveShakes.end(),
			[](UCameraShakeBase* S) { return !S || S->IsFinished(); }),
		ActiveShakes.end());

	// Alpha 가 0 이면 효과 죽음 (Disable 진행 후) — 가산 skip.
	if (Alpha <= 0.0f)
	{
		return false;
	}

	InOutPOV.Location       += Sum.Location * Alpha;
	InOutPOV.Rotation.Pitch += Sum.Rotation.Pitch * Alpha;
	InOutPOV.Rotation.Yaw   += Sum.Rotation.Yaw   * Alpha;
	InOutPOV.Rotation.Roll  += Sum.Rotation.Roll  * Alpha;
	InOutPOV.FOV            += Sum.FOV * Alpha;
	return false;
}

UCameraShakeBase* UCameraModifier_CameraShake::StartShake(
	UClass* ShakeClass,
	float Scale,
	ECameraShakePlaySpace PlaySpace,
	FRotator UserPlaySpaceRot)
{
	if (!ShakeClass) return nullptr;

	UObject* Obj = FObjectFactory::Get().Create(ShakeClass->GetName(), this);
	UCameraShakeBase* Shake = Cast<UCameraShakeBase>(Obj);
	if (!Shake)
	{
		if (Obj) UObjectManager::Get().DestroyObject(Obj);
		return nullptr;
	}

	Shake->StartShake(CameraOwner, Scale, PlaySpace, UserPlaySpaceRot);
	ActiveShakes.push_back(Shake);
	return Shake;
}

void UCameraModifier_CameraShake::StopShake(UCameraShakeBase* ShakeInstance, bool bImmediately)
{
	if (!ShakeInstance) return;
	ShakeInstance->StopShake(bImmediately);
}

void UCameraModifier_CameraShake::StopAllShakes(bool bImmediately)
{
	for (UCameraShakeBase* Shake : ActiveShakes)
	{
		if (Shake) Shake->StopShake(bImmediately);
	}
}

void UCameraModifier_CameraShake::StopAllInstancesOfShake(UClass* ShakeClass, bool bImmediately)
{
	if (!ShakeClass) return;
	for (UCameraShakeBase* Shake : ActiveShakes)
	{
		if (Shake && Shake->GetClass()->IsA(ShakeClass))
		{
			Shake->StopShake(bImmediately);
		}
	}
}
