#pragma once

#include "Object/Object.h"
#include "GameFramework/CameraTypes.h"

class UCameraManager;

// ============================================================
// UCameraShakeBase — 카메라 셰이크의 베이스 클래스
//
// 서브클래스가 데이터 기반(amplitude/frequency 등) 또는 curve 기반으로
// UpdateAndApplyCameraShake 를 구현하면 된다.
//   - 데이터 기반 예시: UPerlinNoiseCameraShake, UWaveOscillatorCameraShake
//   - Curve 기반 예시:  USequenceCameraShake (float curve asset 사용)
//
// 인스턴스는 CameraManager::StartCameraShake 가 UObjectManager 로 생성하고
// 매 프레임 매니저가 UpdateAndApplyCameraShake 를 호출한다. IsFinished()==true
// 가 되면 매니저에서 제거된다.
// UE: UCameraShakeBase
// ============================================================
class UCameraShakeBase : public UObject
{
public:
	DECLARE_CLASS(UCameraShakeBase, UObject)

	UCameraShakeBase() = default;
	~UCameraShakeBase() override = default;

	// 셰이크 시작 시 1회 호출. 패턴 상태 초기화.
	virtual void StartShake(
		UCameraManager* Camera,
		float InScale,
		ECameraShakePlaySpace InPlaySpace,
		FRotator InUserPlaySpaceRot);

	// 매 프레임 호출. OutResult 에 additive 변형을 누적.
	virtual void UpdateAndApplyCameraShake(float DeltaTime, FCameraShakeUpdateResult& OutResult);

	// 종료 요청. 페이드아웃 등 처리 후 IsFinished()==true 가 되면 매니저에서 제거.
	virtual void StopShake(bool bImmediately = true);

	bool IsFinished() const { return bFinished; }

	// 동일 클래스의 인스턴스가 동시에 1개만 살아있게 할지 (UE 의 bSingleInstance)
	bool bSingleInstance = false;

protected:
	UCameraManager* OwnerCameraManager = nullptr;
	float Scale = 1.0f;
	ECameraShakePlaySpace PlaySpace = ECameraShakePlaySpace::CameraLocal;
	FRotator UserPlaySpaceRot;
	bool bFinished = false;
};
