#pragma once

#include "Math/Vector.h"
#include "Math/Rotator.h"

// ============================================================
// FMinimalViewInfo — 카메라 POV "공통 통화"
//
// UE: FMinimalViewInfo (Engine/Source/Runtime/Engine/Classes/Camera/CameraTypes.h)
//
// 생산자:
//   - UCameraComponent::GetCameraView(...)  — 게임/PIE 측
//   - FViewportCameraTransform → 변환     — Editor 측 (예정)
//   - APlayerCameraManager::GetCameraCachePOV(...) — Shake/Fade 후 누적값
//
// 소비자:
//   - FFrameContext::SetCameraInfo(POV)    — 매트릭스/프러스텀 빌드해 렌더 통화로 변환
//
// 매트릭스나 프러스텀은 들고 있지 않는다 — 소비자가 만든다.
// 이 struct 의 목적은 "어디서 무엇을 어떤 시야각으로 보는가" 데이터만 단일화하는 것.
// ============================================================
struct FMinimalViewInfo
{
	FVector  Location     = FVector(0.0f, 0.0f, 0.0f);
	FRotator Rotation;                                // Pitch/Yaw/Roll (degrees)

	// 주의: UE 의 FMinimalViewInfo 는 degrees 지만, KraftonEngine 의 FCameraState/
	// FMatrix::PerspectiveFovLH 가 라디안 기반이라 통화도 라디안으로 통일.
	float    FOV          = 3.14159265358979f / 3.0f; // perspective vertical FOV (radians)
	float    AspectRatio  = 16.0f / 9.0f;
	float    OrthoWidth   = 10.0f;                    // ortho projection 폭

	float    NearClip     = 0.1f;
	float    FarClip      = 1000.0f;

	bool     bIsOrtho     = false;                    // true 면 OrthoWidth 사용, false 면 FOV 사용
};
