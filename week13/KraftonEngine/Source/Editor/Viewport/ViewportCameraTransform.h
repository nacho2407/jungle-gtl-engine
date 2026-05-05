#pragma once

#include "Math/Vector.h"
#include "Math/Rotator.h"

// ============================================================
// FViewportCameraTransform — 에디터 뷰포트 카메라 데이터
//
// UE: FViewportCameraTransform (UnrealEdMisc / EditorViewportClient)
//
// 에디터 뷰포트 카메라는 게임 월드의 액터/컴포넌트가 아니므로 라이프사이클·직렬화·
// GC 영향이 없는 plain struct 로 보유한다. FMinimalViewInfo 로 변환해서 렌더 통화로
// 흘려보낸다.
//
// UE 의 풀 클래스(LookAt/StartLocation/DesiredRotation 등 보간 헬퍼 포함)에서
// 가장 핵심인 데이터 3개만 우선 도입. 보간 로직은 EditorViewportClient 가 보유.
// ============================================================
struct FViewportCameraTransform
{
	FVector  ViewLocation;
	FRotator ViewRotation;
	float    OrthoZoom = 10.0f;     // ortho 모드의 화면 폭 (UE 의 OrthoZoom 등가)
};
