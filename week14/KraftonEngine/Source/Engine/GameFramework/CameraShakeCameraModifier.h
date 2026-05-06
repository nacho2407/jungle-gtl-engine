#pragma once

#include "GameFramework/CameraModifier.h"
#include "GameFramework/CameraTypes.h"

class UCameraShakeBase;
class UClass;

// ============================================================
// UCameraModifier_CameraShake — 모든 active shake 인스턴스를 합산해 POV 에 적용.
//
// PlayerCameraManager 가 기본 modifier 로 1개 보유. StartCameraShake API 가
// 이 modifier 의 ActiveShakes 리스트에 인스턴스를 추가, ModifyCamera 가
// 매 프레임 update + 누적 → POV 에 가산. shake 이 IsFinished() 인 것은 자동 정리.
// UE: UCameraModifier_CameraShake
// ============================================================
class UCameraModifier_CameraShake : public UCameraModifier
{
public:
	DECLARE_CLASS(UCameraModifier_CameraShake, UCameraModifier)

	UCameraModifier_CameraShake();
	~UCameraModifier_CameraShake() override = default;

	bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;

	// PlayerCameraManager::StartCameraShake 가 위임.
	UCameraShakeBase* StartShake(
		UClass* ShakeClass,
		float Scale,
		ECameraShakePlaySpace PlaySpace,
		FRotator UserPlaySpaceRot);

	void StopShake(UCameraShakeBase* ShakeInstance, bool bImmediately);
	void StopAllShakes(bool bImmediately);
	void StopAllInstancesOfShake(UClass* ShakeClass, bool bImmediately);

private:
	TArray<UCameraShakeBase*> ActiveShakes;
};
