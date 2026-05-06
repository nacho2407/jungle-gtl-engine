#pragma once

#include "Object/Object.h"

class APlayerCameraManager;
struct FMinimalViewInfo;

// ============================================================
// UCameraModifier — 카메라 POV 를 in-place 로 변형하는 plug-in.
//
// PlayerCameraManager 가 ModifierList 를 보유하고, base+blend POV 산출 후
// 매 Tick 에 modifier 들을 priority 순으로 호출. 셰이크 / aim assist /
// hit reaction / drunk 효과 등 *카메라에 가하는 효과 단위* 추상화.
//
// 베이스는 Disable/Enable 시 Alpha 페이드 in/out 만 처리한다. 실제 POV
// 변형은 서브클래스가 ModifyCamera 를 override.
// UE: UCameraModifier
// ============================================================
class UCameraModifier : public UObject
{
public:
	DECLARE_CLASS(UCameraModifier, UObject)

	UCameraModifier() = default;
	~UCameraModifier() override = default;

	// 추가 시 1회 호출. 서브클래스가 Owner 캐시 / 초기 상태 설정.
	virtual void AddedToCamera(APlayerCameraManager* InCameraOwner);

	// 매 프레임 호출. POV 를 in-place 로 변형하고, true 반환 시 이 modifier 다음
	// 의 list 를 skip (exclusive). 베이스 구현은 alpha 페이드만 갱신하고 false.
	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV);

	// Disable — bImmediate 이면 즉시 Alpha=0, 아니면 AlphaOutTime 동안 페이드 아웃.
	virtual void DisableModifier(bool bImmediate = false);
	virtual void EnableModifier();

	bool IsDisabled() const { return bDisabled && Alpha <= 0.0f; }

	APlayerCameraManager* CameraOwner = nullptr;

	// Priority 낮은 순서대로 ModifyCamera 호출. 같은 priority 면 추가 순.
	float Priority = 50.0f;
	bool  bExclusive = false;

	// 페이드 인/아웃 시간 (초). 0 이면 즉시 toggle.
	float AlphaInTime = 0.0f;
	float AlphaOutTime = 0.0f;

protected:
	// 0..1 — Disable 시 0 으로 ramp, Enable 시 1 로 ramp. 서브클래스가 효과 강도에 곱.
	float Alpha = 1.0f;
	bool  bDisabled = false;
};
