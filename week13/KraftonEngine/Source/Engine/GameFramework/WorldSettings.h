#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"

/**
 * @brief scene 단위 cloth wind 설정
 */
struct FWorldClothWindSettings
{
	// scene 전체 cloth wind 사용 여부
	bool bEnabled = false;

	// world 기준 wind 방향
	FVector Direction = FVector::ForwardVector;

	// world 기준 wind 세기
	float Strength = 0.0f;

	// 절차적 turbulence 세기
	float TurbulenceStrength = 0.0f;

	// 절차적 turbulence 공간 스케일
	float TurbulenceSpatialScale = 100.0f;

	// 절차적 turbulence 시간 스케일
	float TurbulenceTemporalScale = 1.0f;

	// 절차적 turbulence seed
	int32 TurbulenceSeed = 1337;
};

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

	// scene 전체 cloth component에 공유되는 wind 설정
	FWorldClothWindSettings ClothWind;
};
