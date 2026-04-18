#pragma once

#include "Component/CameraComponent.h"
#include "Core/EngineTypes.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Object/FName.h"

class FLevelEditorViewportClient;

// PIE 세션 실행 위치. 현재는 InProcess만 사용.
enum class EPIESessionDestination : uint8
{
	InProcess,
	// NewProcess, Launcher 등은 UE에 존재하지만 우리는 InProcess만 지원.
};

// PIE 시작 모드. Simulate는 에디터 카메라 유지, Play는 게임 카메라로 전환.
enum class EPIEPlayMode : uint8
{
	PlayInViewport,
	Simulate,
};

// RequestPlaySession에 넘기는 파라미터. UE의 FRequestPlaySessionParams 대응.
struct FRequestPlaySessionParams
{
	EPIESessionDestination SessionDestination = EPIESessionDestination::InProcess;
	EPIEPlayMode PlayMode = EPIEPlayMode::PlayInViewport;
	// PIE를 시작할 대상 뷰포트. null이면 현재 활성 뷰포트를 사용한다.
	FLevelEditorViewportClient* DestinationViewportClient = nullptr;
};

struct FPIEViewportCameraSnapshot
{
	// 현재는 활성 에디터 뷰포트 카메라의 임시 백업.
	// 추후 PIE 시작 위치를 별도 에디터 오브젝트로 지정해도 동일한 스냅샷 구조를 재사용할 수 있다.
	FVector Location;
	FRotator Rotation;
	FCameraState CameraState;
	bool bValid = false;
};

// 활성 PIE 세션 상태. UE의 FPlayInEditorSessionInfo 대응 (최소 버전).
struct FPlayInEditorSessionInfo
{
	FRequestPlaySessionParams OriginalRequestParams;
	double PIEStartTime = 0.0;
	// PIE 시작 직전 활성 월드 핸들 — EndPlayMap에서 원복에 사용.
	FName PreviousActiveWorldHandle;
	// 현재 PIE가 실행 중인 대상 뷰포트.
	FLevelEditorViewportClient* DestinationViewportClient = nullptr;
	FPIEViewportCameraSnapshot SavedViewportCamera;
};
