#include "Game.hpp"

bool Game::Initialize()
{
	renderer.Create(hWnd); // 현재는 공 모델만 사용하기 때문에 정점 버퍼는 Create() 내에서 한 번 생성
	renderer.CreateShader();
	renderer.CreateConstantBuffer();

	QueryPerformanceFrequency(&frequency);

	if (!BlinkInitialized)
	{
		QueryPerformanceCounter(&lastBlinkTime);
		BlinkInitialized = true;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui_ImplWin32_Init((void*)hWnd); // OS 이벤트 및 입력 담당
	ImGui_ImplDX11_Init(renderer.Device, renderer.DeviceContext); // GPU 렌더링 담당

	SoundManager.Initialize();

	//BGM은 mp3파일
	SoundManager.LoadSound(TEXT("./Sound/BGM/Menu.mp3"), true);

	//SFX는 wav파일
	SoundManager.LoadSound(TEXT("./Sound/SFX/Drop.wav"), false);
	SoundManager.LoadSound(TEXT("./Sound/SFX/Merge.wav"), false);
	SoundManager.LoadSound(TEXT("./Sound/SFX/Click.wav"), false);
	SoundManager.LoadSound(TEXT("./Sound/SFX/GameWin.wav"), false);
	SoundManager.LoadSound(TEXT("./Sound/SFX/GameOver.wav"), false);

	SoundManager.PlayBGM(TEXT("./Sound/BGM/Menu.mp3"));

	//연쇄적인 12개의 충돌을 고려해 100개
	ParticlePool.Initialize(100, renderer);

	for (int i = 0; i < ARRAYSIZE(FruitNames); i++)
	{
		std::string FruitPath = "./Resource/";
		FruitPath += FruitNames[i];
		FruitPath += ".png";
		FruitTextures.PushBack(new UTexture(renderer.Device, renderer.DeviceContext, FruitPath.c_str()));
	}

	EmptyTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/preview_empty.png");
	BubbleTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/bubble.png");
	ButtonBGTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/WoodButton_Normal.png");
	ButtonBGHoverTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/WoodButton_Hover.png");
	NextTextTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/NextText.png");
	ScoreTextTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/ScoreText.png");
	Map1Texture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/Map1.png");
	Map1RoofTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/Map1_Roof.png");
	TitleBackgroundTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/title_bg.png");
	RuleTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/rule_bg.png");
	CreditTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/credits.png");
	GameOverTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/gameover_bg.png");
	LeaderBoardBgTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/leaderboard_bg.png");
	DropCatTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/DropCat.png");

	ScoreWidget.SetOffset(FVector(0.7, 0.7, 0));
	ScoreWidget.SetTexture(BubbleTexture);
	ScoreWidget.SetScale(1);
	ScoreWidget.SetAxisScale(FVector::Splat(0.1));

	TestWidget.SetScale(1);
	TestWidget.SetAxisScale(FVector::Splat(0.05));
	TestWidget.SetParent(&ScoreWidget);
	TestWidget.SetOffset(FVector(0, 0, 0));

	NextTextWidget.SetTexture(NextTextTexture);
	NextTextWidget.SetScale(1);
	NextTextWidget.SetAxisScale(FVector::Splat(0.1));
	NextTextWidget.SetParent(&ScoreWidget);
	NextTextWidget.SetOffset(FVector(0, 0.1, 0));

	MapWidget.SetTexture(Map1Texture);
	MapWidget.SetAxisScale(FVector::Splat(1));
	MapWidget.SetOffset(FVector(0, -0.15, 0));

	MapRoofWidget.SetTexture(Map1RoofTexture);
	MapRoofWidget.SetAxisScale(FVector(1, 0.1, 0));
	MapRoofWidget.SetOffset(FVector(0, 0.6, 0));

	LeaderBoardTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/leaderboard.png");
	RankBgTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/rankbg.png");

	RankBg = new UImageWidget[RANK_NUM];
	RankNum = new UTextWidget[RANK_NUM];
	RankScoreIngame = new UTextWidget[RANK_NUM];
	RankScore = new UTextWidget[RANK_NUM];

	LeaderBoardWidget.SetOffset(FVector(-0.8, 0.3, 0));
	LeaderBoardWidget.SetTexture(LeaderBoardTexture);
	LeaderBoardWidget.SetScale(1);
	LeaderBoardWidget.SetAxisScale(FVector::Splat(0.2));


	for (int i = 0; i < RANK_NUM; i++)
	{

		RankBg[i].SetTexture(RankBgTexture);
		RankBg[i].SetScale(0.5);
		RankBg[i].SetAxisScale(FVector::Splat(0.28));
		RankBg[i].SetParent(&LeaderBoardWidget);
		RankBg[i].SetOffset(FVector(0, 0.08 - (0.06 * i), 0));

		RankNum[i].SetNumber(i + 1);
		RankNum[i].SetScale(1);
		RankNum[i].SetParent(&RankBg[i]);
		RankNum[i].SetOffset(FVector(-0.19, 0.015, 0));
		RankNum[i].SetTextColor(DirectX::Colors::Black);

		RankScoreIngame[i].SetScale(1);
		RankScoreIngame[i].SetParent(&RankBg[i]);
		RankScoreIngame[i].SetOffset(FVector(0, 0.015, 0));
		RankScoreIngame[i].SetTextColor(DirectX::Colors::Black);

		RankScore[i].SetScale(2);
		RankScore[i].SetTextColor(DirectX::Colors::Black);
		RankScore[i].SetOffset(FVector(0.1, 0.29 - (0.215 * i), 0));
	}

	LeftScoreWidget.SetOffset(FVector(-0.7f, 0.7f, 0.f));
	LeftScoreWidget.SetTexture(BubbleTexture);
	LeftScoreWidget.SetScale(1.f);
	LeftScoreWidget.SetAxisScale({ 0.1, 0.1f, 1.f });

	ScoreTextWidget.SetTexture(ScoreTextTexture);
	ScoreTextWidget.SetScale(1);
	ScoreTextWidget.SetParent(&LeftScoreWidget);

	ScoreTextWidget.SetAxisScale(FVector::Splat(0.1));
	ScoreTextWidget.SetOffset(FVector(0, 0.1, 0));

	CurrentScoreWidget.SetText(TEXT("0"));
	CurrentScoreWidget.SetParent(&LeftScoreWidget);
	CurrentScoreWidget.SetScale(1.f);
	CurrentScoreWidget.SetOffset(FVector(0.f, 0.0f, 0.f));
	CurrentScoreWidget.SetTextColor(DirectX::Colors::Black);

	GameOverScoreImage.SetOffset(FVector(0.f, -0.2f, 0.f));
	GameOverScoreImage.SetTexture(FruitTextures[10]);
	GameOverScoreImage.SetScale(1.5f);
	GameOverScoreImage.SetAxisScale({ 0.1, 0.1f, 1.f });

	GameOverScoreTextImage.SetTexture(ScoreTextTexture);
	GameOverScoreTextImage.SetScale(1);
	GameOverScoreTextImage.SetParent(&GameOverScoreImage);
	GameOverScoreTextImage.SetAxisScale(FVector::Splat(0.1));
	GameOverScoreTextImage.SetOffset(FVector(0, 0.1, 0));

	GameOverScoreText.SetText(TEXT("0"));
	GameOverScoreText.SetParent(&GameOverScoreImage);
	GameOverScoreText.SetScale(1.f);
	GameOverScoreText.SetTextColor(DirectX::Colors::White);
	GameOverScoreText.SetOffset(FVector(0.f, 0.0f, 0.f));

	RestartButton.SetOffset(FVector(-0.3f, -0.5f, 0.f));
	RestartButton.SetTexture(ButtonBGTexture);
	RestartButton.SetAxisScale({ 0.2, 0.1f, 1.f });

	RestartText.SetParent(&RestartButton);
	RestartText.SetOffset({ 0, 0, 0 });
	RestartText.SetScale(1);
	RestartText.SetText(L"Restart");

	BackToTitleButton.SetOffset(FVector(0.3f, -0.5f, 0.f));
	BackToTitleButton.SetTexture(ButtonBGTexture);
	BackToTitleButton.SetAxisScale({ 0.2, 0.1f, 1.f });

	BackToTitleText.SetParent(&BackToTitleButton);
	BackToTitleText.SetOffset({ 0, 0, 0 });
	BackToTitleText.SetScale(1);
	BackToTitleText.SetText(L"Back To Title");

	StartButton.SetOffset(FVector(0.f, 0.f, 0.f));
	StartButton.SetTexture(ButtonBGTexture);
	StartButton.SetAxisScale({ 0.2, 0.07f, 1.f });

	StartText.SetParent(&StartButton);
	StartText.SetScale(1);
	StartText.SetText(L"Game Start");

	CreditButton.SetParent(&StartButton);
	CreditButton.SetTexture(ButtonBGTexture);
	CreditButton.SetAxisScale({ 0.2, 0.07, 1.f });
	CreditButton.SetOffset({ 0.f, -0.2f, 0.f });

	CreditText.SetParent(&CreditButton);
	CreditText.SetScale(1);
	CreditText.SetText(L"Credits");

	RuleButton.SetParent(&StartButton);
	RuleButton.SetTexture(ButtonBGTexture);
	RuleButton.SetAxisScale({ 0.2, 0.07, 1.f });
	RuleButton.SetOffset({ 0.f, -0.4f, 0.f });

	RuleText.SetParent(&RuleButton);
	RuleText.SetScale(1);
	RuleText.SetText(L"How To Play");

	LeaderBoardButton.SetParent(&StartButton);
	LeaderBoardButton.SetTexture(ButtonBGTexture);
	LeaderBoardButton.SetAxisScale({ 0.2, 0.07, 1.f });
	LeaderBoardButton.SetOffset({ 0.f, -0.6f, 0.f });

	LeaderBoardText.SetParent(&LeaderBoardButton);
	LeaderBoardText.SetScale(1);
	LeaderBoardText.SetText(L"Leader Board");

	StartScreenImage.SetTexture(TitleBackgroundTexture);
	StartScreenImage.SetOffset({ 0, 0, 0 });
	StartScreenImage.SetAxisScale({ 1, 1, 1 });
	StartScreenImage.SetScale(1);

	HowToPlayImage.SetTexture(RuleTexture);
	HowToPlayImage.SetOffset({ 0, 0, 0 });
	HowToPlayImage.SetAxisScale({ 1, 1, 1 });
	HowToPlayImage.SetScale(1);

	CreditImage.SetTexture(CreditTexture);
	CreditImage.SetOffset({ 0, 0, 0 });
	CreditImage.SetAxisScale({ 1, 1, 1 });
	CreditImage.SetScale(1);

	GameOverImage.SetTexture(GameOverTexture);
	GameOverImage.SetOffset({ 0, 0, 0 });
	GameOverImage.SetAxisScale({ 1, 1, 1 });
	GameOverImage.SetScale(1);

	LeaderBoardImage.SetTexture(LeaderBoardBgTexture);
	LeaderBoardImage.SetOffset({ 0, 0, 0 });
	LeaderBoardImage.SetAxisScale({ 1, 1, 1 });
	LeaderBoardImage.SetScale(1);

	BackButton.SetOffset(FVector(-0.8f, 0.8f, 0.f));
	BackButton.SetTexture(ButtonBGTexture);
	BackButton.SetAxisScale({ 0.1, 0.05f, 1.f });

	BackText.SetParent(&BackButton);
	BackText.SetOffset({ 0, 0, 0 });
	BackText.SetScale(1);
	BackText.SetText(L"Back");

	DropCatImage.SetTexture(DropCatTexture);
	DropCatImage.SetOffset({ 0, 0, 0 });
	DropCatImage.SetAxisScale({ 0.15, 0.15, 0.2 });
	DropCatImage.SetScale(1.2);

	QueryPerformanceCounter(&LastSpawnTime);

	CurrentFruitType = GetRandomFruitType();
	NextFruitType = GetRandomFruitType();

	GameTime = 0;

	return true;
}

void Game::Run()
{
	while (true)
	{
		QueryPerformanceCounter(&startTime);

		MSG msg;

		// ----- input ------
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);

			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
			{
				return;
			}
		}

		InputManager.Update(hWnd, fixedDeltaTime);

		Update();

		// 모니터 주사율이 144Hz일 수도 있으니 VSync를 사용하더라도 60fps 고정 코드는 유지
		do
		{
			Sleep(0);

			QueryPerformanceCounter(&endTime);

			elapsedTime = (endTime.QuadPart - startTime.QuadPart) * 1000.0 / frequency.QuadPart;
		} while (elapsedTime < targetFrameTime);

		// 사실 elapsedTime 이 더 정확하긴 한데, 다른 Update 에서 fixedDeltaTime 을 기준으로 하고 있어서 일관성 유지
		GameTime += fixedDeltaTime;
	}
}

void Game::ShutDown()
{
	SoundManager.StopAll();
	SoundManager.Release();

	//파티클 텍스쳐도 삭제
	ParticlePool.ReleasePool();

	for (int i = 0; i < FruitTextures.Num(); i++)
	{
		// Release와 동시에 FruitTextures[i] 자체도 해제
		delete FruitTextures[i];
	}

	for (UINT i = 0; i < PrimitiveList.Num(); i++) {
		delete PrimitiveList[i];
		PrimitiveList[i] = nullptr;
	}
	PrimitiveList.Clear();

	// 임시 메모리 해제
	delete LeaderBoardTexture;
	delete RankBgTexture;
	delete[] RankBg;
	delete[] RankNum;
	delete[] RankScoreIngame;
	delete[] RankScore;

	// 텍스처 해제
	delete EmptyTexture;
	delete BubbleTexture;
	delete ButtonBGTexture;
	delete ButtonBGHoverTexture;
	delete NextTextTexture;
	delete ScoreTextTexture;
	delete Map1Texture;
	delete Map1RoofTexture;
	delete TitleBackgroundTexture;
	delete RuleTexture;
	delete CreditTexture;
	delete GameOverTexture;
	delete LeaderBoardBgTexture;
	delete DropCatTexture;

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	renderer.ReleaseConstantBuffer();
	renderer.ReleaseShader();
	renderer.Release();
}

void Game::Update()

{
	switch (GameState)
	{
	case GS_Title:
		UpdateTitle();
		break;

	case GS_Credits:
		UpdateCredits();
		break;

	case GS_HowToPlay:
		UpdateHowToPlay();
		break;

	case GS_LeaderBoard:
		UpdateLeaderBoard();
		break;

	case GS_Playing:
		UpdatePlaying();
		break;

	case GS_GameOver:
		UpdateGameOver();
		break;
	}

	renderer.SwapBuffer();
}

void Game::UpdateTitle()
{
	renderer.Prepare();
	renderer.PrepareShader();

	if (InputManager.IsXPadConnected())
	{
		if (InputManager.IsXPadHoldingRepeatTriggered())
		{
			if (InputManager.GetXPadStickY() < 0)
			{
				XPadButtonIndex = (XPadButtonIndex + 1) % 4;
			}
			else
			{
				if (XPadButtonIndex == 0) XPadButtonIndex = 3;
				else XPadButtonIndex -= 1;
			}
		}

		// 모든 버튼 Hover -> Normal
		// 비효율적이긴한데 시간이 없어서 우선 이렇게 처리
		StartButton.SetTexture(ButtonBGTexture);
		CreditButton.SetTexture(ButtonBGTexture);
		RuleButton.SetTexture(ButtonBGTexture);
		LeaderBoardButton.SetTexture(ButtonBGTexture);

		switch (XPadButtonIndex)
		{
		case 0:
			StartButton.SetTexture(ButtonBGHoverTexture);
			break;

		case 1:
			CreditButton.SetTexture(ButtonBGHoverTexture);
			break;

		case 2:
			RuleButton.SetTexture(ButtonBGHoverTexture);
			break;
		case 3:
			LeaderBoardButton.SetTexture(ButtonBGHoverTexture);
			break;
		}

		if (InputManager.IsXPadAReleased())
		{
			switch (XPadButtonIndex)
			{
			case 0:
				GameState = GS_Playing;
				break;

			case 1:
				GameState = GS_Credits;
				break;

			case 2:
				GameState = GS_HowToPlay;
				break;
			case 3:
				GameState = GS_LeaderBoard;
				break;
			}
		}
	}
	else
	{
		if (StartButton.IsOnBound(hWnd))
			StartButton.SetTexture(ButtonBGHoverTexture);
		else
			StartButton.SetTexture(ButtonBGTexture);

		if (CreditButton.IsOnBound(hWnd))
			CreditButton.SetTexture(ButtonBGHoverTexture);
		else
			CreditButton.SetTexture(ButtonBGTexture);

		if (RuleButton.IsOnBound(hWnd))
			RuleButton.SetTexture(ButtonBGHoverTexture);
		else
			RuleButton.SetTexture(ButtonBGTexture);

		if (LeaderBoardButton.IsOnBound(hWnd))
			LeaderBoardButton.SetTexture(ButtonBGHoverTexture);
		else
			LeaderBoardButton.SetTexture(ButtonBGTexture);

		if (StartButton.IsPressed(hWnd, InputManager, SoundManager))
		{
			GameState = GS_Playing;
		}
		else if (CreditButton.IsPressed(hWnd, InputManager, SoundManager))
		{
			GameState = GS_Credits;
		}
		else if (RuleButton.IsPressed(hWnd, InputManager, SoundManager))
		{
			GameState = GS_HowToPlay;
		}
		else if (LeaderBoardButton.IsPressed(hWnd, InputManager, SoundManager))
		{
			GameState = GS_LeaderBoard;
		}
	}

	StartScreenImage.Render(renderer);
	StartButton.Render(renderer);
	CreditButton.Render(renderer);
	RuleButton.Render(renderer);
	LeaderBoardButton.Render(renderer);

	StartText.Render(renderer);
	CreditText.Render(renderer);
	RuleText.Render(renderer);
	LeaderBoardText.Render(renderer);
}

void Game::UpdateCredits()
{
	renderer.Prepare();
	renderer.PrepareShader();

	if (InputManager.IsXPadConnected())
	{
		BackButton.SetTexture(ButtonBGHoverTexture);
		if (InputManager.IsXPadAReleased())
		{
			GameState = GS_Title;
		}
	}
	else
	{
		if (BackButton.IsOnBound(hWnd))
			BackButton.SetTexture(ButtonBGHoverTexture);
		else
			BackButton.SetTexture(ButtonBGTexture);

		if (BackButton.IsPressed(hWnd, InputManager, SoundManager))
		{
			GameState = GS_Title;
		}
	}

	CreditImage.Render(renderer);

	BackButton.Render(renderer);
	BackText.Render(renderer);
}

void Game::UpdateHowToPlay()
{
	renderer.Prepare();
	renderer.PrepareShader();

	if (InputManager.IsXPadConnected())
	{
		BackButton.SetTexture(ButtonBGHoverTexture);
		if (InputManager.IsXPadAReleased())
		{
			GameState = GS_Title;
		}
	}
	else
	{
		if (BackButton.IsOnBound(hWnd))
			BackButton.SetTexture(ButtonBGHoverTexture);
		else
			BackButton.SetTexture(ButtonBGTexture);

		if (BackButton.IsPressed(hWnd, InputManager, SoundManager))
		{
			GameState = GS_Title;
		}
	}

	HowToPlayImage.Render(renderer);
	BackButton.Render(renderer);
	BackText.Render(renderer);
}

void Game::UpdateLeaderBoard()
{
	renderer.Prepare();
	renderer.PrepareShader();

	if (InputManager.IsXPadConnected())
	{
		BackButton.SetTexture(ButtonBGHoverTexture);
		if (InputManager.IsXPadAReleased())
		{
			GameState = GS_Title;
		}
	}
	else
	{
		if (BackButton.IsOnBound(hWnd))
			BackButton.SetTexture(ButtonBGHoverTexture);
		else
			BackButton.SetTexture(ButtonBGTexture);

		if (BackButton.IsPressed(hWnd, InputManager, SoundManager))
		{
			GameState = GS_Title;
		}
	}

	LeaderBoardImage.Render(renderer);
	BackButton.Render(renderer);
	BackText.Render(renderer);

	for (int i = 0; i < RANK_NUM; i++)
	{
		RankScore[i].SetNumber(ScoreManager.GetHighScores()[i]);
		RankScore[i].Render(renderer);
	}
}

void Game::UpdatePlaying()
{
	bool spawnFruit = false;

	if (InputManager.IsXPadConnected())
	{
		if (InputManager.IsXPadTriggerDown())
			spawnX += InputManager.GetXPadStickX() * 0.01f;
		else
			spawnX += InputManager.GetXPadStickX() * 0.03f;

		spawnX = max(min(spawnX, 1), -1);

		if (bIsVibrating)
		{
			VibrationTime += fixedDeltaTime;
			if (VibrationTime >= MaxVibrationTime)
			{
				// 진동 종료
				XINPUT_VIBRATION vibration{};
				vibration.wLeftMotorSpeed = 0;
				vibration.wRightMotorSpeed = 0;
				XInputSetState(0, &vibration);
				bIsVibrating = false;
				VibrationTime = 0;
			}
		}

		bool curMouseDown = InputManager.IsXPadADown();

		if (curMouseDown && !prevMouseDown)
		{
			LARGE_INTEGER currentTime;
			QueryPerformanceCounter(&currentTime);

			float timeSinceLastSpawn = (currentTime.QuadPart - LastSpawnTime.QuadPart) / (float)frequency.QuadPart;
			if (timeSinceLastSpawn >= SpawnCooldown)
			{
				spawnFruit = true;
				LastSpawnTime = currentTime;
			}
		}

		prevMouseDown = curMouseDown;
	}
	else
	{
		POINT mousePos = InputManager.GetMousePosition();

		spawnX = mousePos.x / renderer.ViewportInfo.Width;
		spawnX = max(0.0f, min(1.0f, spawnX));
		spawnX = spawnX * 2.0f - 1.0f;

		// 화면 바깥을 클릭해도 과일이 생성되는 문제 해결
		bool isMouseInScene = (mousePos.y >= renderer.ViewportInfo.TopLeftY) && (mousePos.y <= renderer.ViewportInfo.TopLeftY + renderer.ViewportInfo.Height) &&
			(mousePos.x >= renderer.ViewportInfo.TopLeftX) && (mousePos.x <= renderer.ViewportInfo.TopLeftX + renderer.ViewportInfo.Width);

		bool curMouseDown = InputManager.IsMouseDown();

		if (curMouseDown && !prevMouseDown && isMouseInScene)
		{
			LARGE_INTEGER currentTime;
			QueryPerformanceCounter(&currentTime);

			float timeSinceLastSpawn = (currentTime.QuadPart - LastSpawnTime.QuadPart) / (float)frequency.QuadPart;
			if (timeSinceLastSpawn >= SpawnCooldown)
			{
				spawnFruit = true;
				LastSpawnTime = currentTime;
			}
		}

		prevMouseDown = curMouseDown;
	}

	renderer.Prepare();
	renderer.PrepareShader();

	SoundManager.Update();

	MapWidget.Render(renderer);

	EFruitType ft = (EFruitType)CurrentFruitType;

	// 새 과일 생성
	if (spawnFruit)
	{
		EFruitType ft = (EFruitType)CurrentFruitType;

		float offsetAngle = ((float)rand() / RAND_MAX) * 2.0f * 3.14159f;
		float offsetDist = 0.01f;
		spawnX += cosf(offsetAngle) * offsetDist;

		FVector spawnPos = FVector(spawnX, 0.9f, 0);

		SoundManager.PlaySFX(TEXT("./Sound/SFX/Drop.wav"), 1.f);
		PrimitiveList.PushBack(CreateFruitAt(ft, spawnPos, FruitTextures[ft]));

		// 다음 과일
		CurrentFruitType = NextFruitType;
		NextFruitType = GetRandomFruitType();
	}

	previewFruit.SetFruitType((EFruitType)CurrentFruitType);
	if (spawnX < ((-1) * MapBoarder) + previewFruit.Radius) { spawnX = ((-1) * MapBoarder) + previewFruit.Radius; }
	else if (spawnX > MapBoarder - previewFruit.Radius) { spawnX = MapBoarder - previewFruit.Radius; }
	previewFruit.SetLocation(FVector(spawnX, 0.8f, 0));
	DropCatImage.SetOffset(FVector(spawnX, 0.85f, 0));

	// 과일이 생성 가능한 상태인지에 따라 프리뷰 이미지 변경
	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);

	float timeSinceLastSpawn = (currentTime.QuadPart - LastSpawnTime.QuadPart) / (float)frequency.QuadPart;
	if (timeSinceLastSpawn >= SpawnCooldown)
	{
		previewFruit.SetTexture(FruitTextures[CurrentFruitType]);
	}
	else
	{
		previewFruit.SetTexture(EmptyTexture);
	}

	DropCatImage.Render(renderer);
	previewFruit.Render(renderer);

	TestWidget.SetTexture(FruitTextures[NextFruitType]);
	// 병합 예정 정보를 담은 큐
	TQueue<FPendingMerge> PendingMergeQueue;

	for (UINT i = 0; i < PrimitiveList.Num(); i++)
	{
		PrimitiveList[i]->UpdatePhysics(fixedDeltaTime);
	}
	ParticlePool.Update(fixedDeltaTime);

	// 과일들 간 충돌 / 병합 연산
	for (UINT i = 0; i < PrimitiveList.Num(); i++)
	{
		UFruit* A = (UFruit*)PrimitiveList[i];

		if (A->IsPendingDestroy())
		{
			continue;
		}

		for (UINT j = i + 1; j < PrimitiveList.Num(); j++)
		{
			UFruit* B = (UFruit*)PrimitiveList[j];

			if (B->IsPendingDestroy())
			{
				continue;
			}

			FVector Delta = B->Location - A->Location;
			float Dist2 = Delta.Dot2(Delta);
			float r = A->Radius + B->Radius;

			if (Dist2 <= r * r + MergeSlop)
			{
				if (A->GetFruitType() == B->GetFruitType())
				{
					if (InputManager.IsXPadConnected())
					{
						// 패드 연결 상태
						XINPUT_VIBRATION vibration{};
						vibration.wLeftMotorSpeed = 500 * (A->GetFruitType() * A->GetFruitType() + 1);
						vibration.wRightMotorSpeed = 500 * (A->GetFruitType() * A->GetFruitType() + 1);

						XInputSetState(0, &vibration);

						bIsVibrating = true;
						VibrationTime = 0.f;
					}

					A->MarkPendingDestroy();
					B->MarkPendingDestroy();

					// 수박끼리 병합 시 두 수박만 사라지고 병합 X
					if (A->GetFruitType() != EFruitType::FT_Watermelon) {
						//FVector TargetLocation = (A->Location.y < B->Location.y) ? A->Location : B->Location;
						FVector TargetLocation = A->Location;

						PendingMergeQueue.Push({
							(EFruitType)(A->GetFruitType() + 1),
							TargetLocation
							});

						//병합된 곳에서 파티클 생성
						ParticlePool.Aquire(TargetLocation);
						SoundManager.PlaySFX(TEXT("./Sound/SFX/Merge.wav"));

					}
					else
					{
						ScoreManager.AddScore(EFruitType::FT_Watermelon);
						ParticlePool.Aquire(A->Location);
						SoundManager.PlaySFX(TEXT("./Sound/SFX/Merge.wav"));
					}
				}
			}
		}
	}

	// 과일 삭제 로직
	for (UINT i = 0; i < PrimitiveList.Num();)
	{
		if (PrimitiveList[i]->IsPendingDestroy())
		{
			delete PrimitiveList[i];
			PrimitiveList[i] = nullptr;
			PrimitiveList.RemoveAt(i);
		}
		else
		{
			i++;
		}
	}

	// 병합 처리 로직
	while (!PendingMergeQueue.IsEmpty())
	{
		FPendingMerge MergeInfo = PendingMergeQueue.Front();
		PendingMergeQueue.Pop();

		PrimitiveList.PushBack(CreateFruitAt(MergeInfo.ResultFruitType, MergeInfo.NewLocation, FruitTextures[MergeInfo.ResultFruitType]));
		
		if (MergeInfo.ResultFruitType == EFruitType::FT_Watermelon)
		{
			SoundManager.PlaySFX(TEXT("./Sound/SFX/GameWin.wav"));
		}

		// 점수 획득
		ScoreManager.AddScore(MergeInfo.ResultFruitType);
	}

	CurrentScoreWidget.SetText(std::to_wstring(ScoreManager.GetScore()).c_str());

	for (UINT i = 0; i < overlapIteration; ++i)
	{
		for (UINT i = 0; i < PrimitiveList.Num(); i++)
		{
			UFruit* A = (UFruit*)PrimitiveList[i];

			for (UINT j = i + 1; j < PrimitiveList.Num(); j++)
			{
				UFruit* B = (UFruit*)PrimitiveList[j];

				ResolveCollision(A, B);
			}
		}
	}

	for (UINT i = 0; i < PrimitiveList.Num(); i++)
	{
		PrimitiveList[i]->Render(renderer);
	}

	ParticlePool.Render(renderer);

	bool bLineCollision = false;

	for (UINT i = 0; i < PrimitiveList.Num(); i++)
	{
		UFruit* fruit = (UFruit*)PrimitiveList[i];

		if (fruit->CheckLineCollision(LineY))
		{
			bLineCollision = true;

			if (!prevCollision)
			{
				QueryPerformanceCounter(&collisionStartCounter);
			}

			break;
		}
	}
	// Danger 여부 계산
	if (bLineCollision)
	{
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);

		double elapsed = (double)(now.QuadPart - collisionStartCounter.QuadPart) / frequency.QuadPart;

		if (elapsed >= DangerLimit)
		{
			LARGE_INTEGER now;
			QueryPerformanceCounter(&now);

			double blinkElapsed = (double)(now.QuadPart - lastBlinkTime.QuadPart) / frequency.QuadPart;

			if (blinkElapsed >= BlinkInterval)
			{
				if (MapRoofWidget.GetTintColor().y == 0 &&
					MapRoofWidget.GetTintColor().z == 0)
				{
					MapRoofWidget.SetTintColor(FVector(1, 1, 1));
				}
				else
				{
					MapRoofWidget.SetTintColor(FVector(1, 0, 0));
				}

				lastBlinkTime = now;
			}
		}
	}
	else
	{
		MapRoofWidget.SetTintColor(FVector(1, 1, 1));
	}
	// Game Over 여부 계산
	if (bLineCollision)
	{
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);

		double elapsed = (double)(now.QuadPart - collisionStartCounter.QuadPart) / frequency.QuadPart;

		if (elapsed >= GameOverLimit)
		{
			// 데이터 초기화. 게임 매니저가 있으면 UpdateGameOver 함수에서 GameState가 GS_Playing으로 바뀔 때 초기화하는 쪽으로 리팩토링
			for (UINT i = 0; i < PrimitiveList.Num(); i++)
			{
				delete PrimitiveList[i];
				PrimitiveList[i] = nullptr;
			}
			PrimitiveList.Clear();

			ParticlePool.Reset();

			prevCollision = false;

			prevMouseDown = false;

			CurrentFruitType = GetRandomFruitType();
			NextFruitType = GetRandomFruitType();

			SoundManager.StopAll();
			SoundManager.PlaySFX(TEXT("./Sound/SFX/GameOver.wav"));

			GameState = GS_GameOver;

			return;
		}
	}

	prevCollision = bLineCollision;

	// 디버깅 UI
	/*
	ImGui_ImplDX11_NewFrame(); // 렌더러 쪽 준비
	ImGui_ImplWin32_NewFrame(); // 플랫폼 쪽 준비
	ImGui::NewFrame(); // ImGui Core 초기화

	ImGui::Begin("Team4 Game");

	ImGui::Combo("Fruit Type", &CurrentFruitType, FruitNames, sizeof(FruitNames) / sizeof(*FruitNames));
	ImGui::Text("Score: %d", ScoreManager.GetScore());
	if (ImGui::Button("Give Up"))
	{
		SoundManager.StopAll();
		SoundManager.PlayBGM(TEXT("./Sound/SFX/GameOver.wav"));
		GameState = GS_GameOver;
	}

	ImGui::End();
	ImGui::Render(); // 이번 프레임에 그릴 UI 설정
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); // DX11 백 엔드를 통해 실제 GPU 드로우 호출
	*/

	MapRoofWidget.Render(renderer);

	LeftScoreWidget.SetOffset(FVector(-0.85f, 0.7f + 0.01 * sinf(4 * GameTime), 0.f));
	LeftScoreWidget.Render(renderer);
	ScoreTextWidget.Render(renderer);

	ScoreWidget.SetOffset(FVector(0.85f, 0.7 + 0.01 * sinf(4 * GameTime), 0));
	ScoreWidget.Render(renderer);
	TestWidget.Render(renderer);
	NextTextWidget.Render(renderer);


	LeaderBoardWidget.Render(renderer);

	for (int i = 0; i < RANK_NUM; i++)
	{
		RankBg[i].Render(renderer);
	}
	for (int i = 0; i < RANK_NUM; i++)
	{
		RankNum[i].Render(renderer);
		RankScoreIngame[i].SetNumber(ScoreManager.GetHighScores()[i]);
		RankScoreIngame[i].Render(renderer);

	}

	NextTextWidget.Render(renderer);
	CurrentScoreWidget.Render(renderer);
}

void Game::UpdateGameOver()
{
	renderer.Prepare();
	renderer.PrepareShader();

	if (InputManager.IsXPadConnected())
	{
		if (InputManager.IsXPadHoldingRepeatTriggered())
		{
			if (InputManager.GetXPadStickX() > 0)
			{
				XPadButtonIndex = (XPadButtonIndex + 1) % 2;
			}
			else
			{
				if (XPadButtonIndex == 0) XPadButtonIndex = 1;
				else XPadButtonIndex -= 1;
			}
		}

		// 모든 버튼 Hover -> Normal
		// 비효율적이긴한데 시간 문제로...
		RestartButton.SetTexture(ButtonBGTexture);
		BackToTitleButton.SetTexture(ButtonBGTexture);

		switch (XPadButtonIndex)
		{
		case 0:
			RestartButton.SetTexture(ButtonBGHoverTexture);
			break;

		case 1:
			BackToTitleButton.SetTexture(ButtonBGHoverTexture);
			break;
		}

		if (InputManager.IsXPadAReleased())
		{
			switch (XPadButtonIndex)
			{
			case 0:
				SoundManager.PlayBGM(TEXT("./Sound/BGM/Menu.mp3"));
				GameState = GS_Playing;
				ScoreManager.UpdateHighScore();
				ScoreManager.Reset();
				break;

			case 1:
				SoundManager.PlayBGM(TEXT("./Sound/BGM/Menu.mp3"));
				GameState = GS_Title;
				ScoreManager.UpdateHighScore();
				ScoreManager.Reset();
				XPadButtonIndex = 0;
				break;
			}
		}
	}
	else
	{
		if (RestartButton.IsOnBound(hWnd))
			RestartButton.SetTexture(ButtonBGHoverTexture);
		else
			RestartButton.SetTexture(ButtonBGTexture);

		if (BackToTitleButton.IsOnBound(hWnd))
			BackToTitleButton.SetTexture(ButtonBGHoverTexture);
		else
			BackToTitleButton.SetTexture(ButtonBGTexture);

		if (RestartButton.IsPressed(hWnd, InputManager, SoundManager))
		{
			SoundManager.PlayBGM(TEXT("./Sound/BGM/Menu.mp3"));
			GameState = GS_Playing;
			ScoreManager.UpdateHighScore();
			ScoreManager.Reset();
		}
		else if (BackToTitleButton.IsPressed(hWnd, InputManager, SoundManager))
		{
			SoundManager.PlayBGM(TEXT("./Sound/BGM/Menu.mp3"));
			GameState = GS_Title;
			ScoreManager.UpdateHighScore();
			ScoreManager.Reset();
		}
	}

	GameOverImage.Render(renderer);
	RestartButton.Render(renderer);
	BackToTitleButton.Render(renderer);

	GameOverScoreImage.Render(renderer);
	GameOverScoreTextImage.Render(renderer);

	RestartText.Render(renderer);
	BackToTitleText.Render(renderer);

	GameOverScoreText.SetText(std::to_wstring(ScoreManager.GetScore()).c_str());
	GameOverScoreText.Render(renderer);

	// 종료 시 진동 종료
	if (bIsVibrating)
	{
		// 진동 종료
		XINPUT_VIBRATION vibration{};
		vibration.wLeftMotorSpeed = 0;
		vibration.wRightMotorSpeed = 0;
		XInputSetState(0, &vibration);
		bIsVibrating = false;
		VibrationTime = 0;
	}
}
