#pragma once

#include "Core/EngineTypes.h"
#include "Object/FName.h"

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
};

// 활성 PIE 세션 상태. UE의 FPlayInEditorSessionInfo 대응 (최소 버전).
struct FPlayInEditorSessionInfo
{
	FRequestPlaySessionParams OriginalRequestParams;
	double PIEStartTime = 0.0;
	// PIE 시작 직전 활성 월드 핸들 — EndPlayMap에서 원복에 사용.
	FName PreviousActiveWorldHandle;
};
