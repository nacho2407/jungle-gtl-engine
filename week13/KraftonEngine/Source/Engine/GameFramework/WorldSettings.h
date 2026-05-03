#pragma once

#include "Core/CoreTypes.h"

// ============================================================
// FWorldSettings — UWorld 단위 (= Scene 파일 단위) 의 게임 설정.
//
// 의도: ProjectSettings 가 "프로젝트 전역 설정" 이라면 WorldSettings 는 "이 씬 한정 설정".
// 예: 이 씬은 어떤 GameMode 를 쓸지 (Intro.Scene = AGameModeIntro / Map.Scene =
//     AGameModeCarGame). 향후 spawn 포지션, 기본 fog, 매치 시간 등도 여기에 누적.
//
// SceneSaveManager 가 scene JSON 의 "WorldSettings" 객체로 직렬화. 비어있으면
// 호출자 측 default (UGameEngine 의 ProjectSettings → AGameModeCarGame fallback) 가 적용.
// ============================================================
struct FWorldSettings
{
	// 비우면 ProjectSettings.GameModeClassName 또는 코드 default 가 fallback.
	// 채우면 LoadSceneFromPath 가 UClass::FindByName 으로 resolve.
	FString GameModeClassName;
};
