#pragma once

#include <windows.h>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"

#include "stb_image.h"

#include "Constants.h"
#include "Physics.hpp"

#include "EFruitType.h"
#include "EGameState.h"

#include "UFruit.hpp"
#include "UInputManager.hpp"
#include "UParticlePool.hpp"
#include "URenderer.hpp"
#include "UScoreManager.hpp"
#include "USoundManager.hpp"
#include "UTexture.hpp"
#include "Widgets.hpp"

class Game
{
public:
	Game(HWND hWnd) : hWnd(hWnd) {}

	bool Initialize();

	void Run();

	void ShutDown();

private:
	// 60fps 고정 상황 가정
	const int targetFPS = 60;
	const double targetFrameTime = 1000.0 / targetFPS;
	const float fixedDeltaTime = (float)targetFrameTime / 1000;

	const char* const FruitNames[EFruitType::FT_Count] = {
		"Cherry", "Strawberry", "Grape", "Dekopon", "Orange",
		"Apple", "Pear", "Peach", "Pineapple", "Melon", "Watermelon"
	};

	// 과일 생성 간 최소 시간 간격 (초)
	const float SpawnCooldown = 0.5f;

	// 과일 간 충돌 계산 시 기준이 너무 빡빡하면 가끔씩 충돌이 감지되지 않는 경우가 있어서 약간의 여유를 둠
	const float MergeSlop = 0.005f;

	// 과일 간 충돌 해소 반복 횟수
	const static int overlapIteration = 100;

	struct FPendingMerge
	{
		EFruitType ResultFruitType;
		FVector NewLocation;
	};

	HWND hWnd;

	UInputManager InputManager;
	UParticlePool ParticlePool;
	URenderer renderer;
	UScoreManager ScoreManager;
	USoundManager SoundManager;

	EGameState GameState = GS_Title;

	TArray<UPrimitive*> PrimitiveList;
	TArray<UTexture*> FruitTextures;

	UFruit previewFruit;

	LARGE_INTEGER frequency;
	LARGE_INTEGER startTime, endTime;
	LARGE_INTEGER collisionStartCounter;

	LARGE_INTEGER lastBlinkTime;
	LARGE_INTEGER LastSpawnTime;

	double elapsedTime = 0.0;

	bool BlinkInitialized = false;
	bool prevCollision = false; // 이전 프레임의 선 충돌 여부
	bool prevMouseDown = false; // 이전 프레임 마우스 클릭 여부

	UTexture* EmptyTexture = nullptr;
	UTexture* BubbleTexture = nullptr;
	UTexture* ButtonBGTexture = nullptr;
	UTexture* ButtonBGHoverTexture = nullptr;
	UTexture* NextTextTexture = nullptr;
	UTexture* ScoreTextTexture = nullptr;
	UTexture* Map1Texture = nullptr;
	UTexture* Map1RoofTexture = nullptr;
	UTexture* TitleBackgroundTexture = nullptr;
	UTexture* RuleTexture = nullptr;
	UTexture* CreditTexture = nullptr;
	UTexture* GameOverTexture = nullptr;
	UTexture* LeaderBoardTexture = nullptr;
	UTexture* RankBgTexture = nullptr;
	UTexture* LeaderBoardBgTexture = nullptr;
	UTexture* DropCatTexture = nullptr;

	UImageWidget ScoreWidget, TestWidget, NextTextWidget;
	UTextWidget TestTextWidget;
	UImageWidget MapWidget;
	UImageWidget MapRoofWidget;
	UImageWidget LeaderBoardWidget;
	UImageWidget* RankBg = nullptr;
	UTextWidget* RankNum = nullptr;
	UTextWidget* RankScoreIngame = nullptr;
	UTextWidget* RankScore = nullptr;
	UImageWidget LeftScoreWidget, ScoreTextWidget; //왼쪽 점수
	UTextWidget  CurrentScoreWidget;
	UImageWidget GameOverScoreImage, GameOverScoreTextImage; // 게임 오버 점수 UI
	UTextWidget  GameOverScoreText;
	UButtonWidget StartButton, CreditButton, RuleButton, LeaderBoardButton, BackButton, RestartButton, BackToTitleButton;
	UTextWidget StartText, CreditText, RuleText, LeaderBoardText, BackText, RestartText, BackToTitleText;
	UImageWidget StartScreenImage, HowToPlayImage, CreditImage, GameOverImage, LeaderBoardImage, DropCatImage;

	float spawnX = 0;
	float VibrationTime = 0.f;
	const float MaxVibrationTime = 0.3f;
	bool bIsVibrating = false;
	int XPadButtonIndex = 0; // GameState 변화 간 Index 를 초기화해줘야 하는데 지금 구조 상으로 아직 안돼있어서 나중에 고쳐야할듯함

	int CurrentFruitType;
	int NextFruitType;

	float GameTime;

private:
	EFruitType GetRandomFruitType()
	{
		return static_cast<EFruitType>(rand() % (EFruitType::FT_Orange + 1));
	}

	void Update();

	void UpdateTitle();

	void UpdateCredits();

	void UpdateHowToPlay();

	void UpdateLeaderBoard();

	void UpdatePlaying();

	void UpdateGameOver();
};
