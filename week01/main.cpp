#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "Xinput.lib")

#pragma comment (lib, "fmod_vc.lib")

#include <windows.h>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "Constants.h"
#include "Physics.hpp"

#include "EFruitType.h"

#include "UFruit.hpp"
#include "UInputManager.hpp"
#include "UParticlePool.hpp"
#include "URenderer.hpp"
#include "UScoreManager.hpp"
#include "USoundManager.hpp"
#include "UTexture.hpp"

struct SceneFrame
{
	ImVec2 Pos;
	ImVec2 Size;
};

class UWidget
{
public:
	UWidget() :
		Parent(nullptr),
		Scale(1)
	{

	}

	void SetParent(UWidget* InWidget)
	{
		// Offset 은 Scale = 1 기준이므로 Scale 을 배제한 GetLocation 사용
		if (InWidget)
		{
			FVector PrevLocation = GetLocation(false);
			Parent = InWidget;
			Offset = PrevLocation - Parent->GetLocation(false);
		}
		else
		{
			if (Parent)
			{
				// parent 초기화하기 전에 현재 위치를 Offset 으로 지정 (parent == nullptr 일 때 Offset 은 location 임을 기억)
				Offset = GetLocation(false);
				Parent = nullptr;
			}
			else
			{
				// do nothing
			}
		}
	}

	void SetScale(float InScale) { Scale = InScale; }

	// Scale = 1 기준, Parent 와의 Offset 설정
	// Parent == nullptr 인 경우 Location 설정과 같음
	// Offset = Self.Loc - Parent.Loc
	// Self.Loc = Parent.Loc + Offset
	void SetOffset(FVector InOffset) { Offset = InOffset; }

	FVector GetLocation(bool bApplyScale) const
	{
		if (Parent)
		{
			FVector Location = Parent->GetLocation(bApplyScale);
			if (bApplyScale) Location = Location + Offset * GetParentScale();
			return Location;
		}
		else
		{
			return Offset;
		}
	}

	float GetParentScale() const
	{
		if (Parent)
		{
			return Parent->GetScale() * Parent->GetParentScale();
		}
		else
		{
			return 1;
		}
	}

	float GetScale() const
	{
		return Scale;
	}

	virtual void Render(URenderer& renderer)
	{

	}

protected:
	// Cartesian <-> Screen 변환 과정을 어느 객체에서 또는 함수에서 할 것인가는 논의 필요
	FVector CartesianToScreenCoord(FVector CartesianCoord)
	{
		// Screen Size 가 (1024, ) 고정 상황 가정
		return FVector((CartesianCoord.x + 1.0f) * 0.5f * 1024, (1 - CartesianCoord.y) * 0.5f * 1024);
	}

private:
	UWidget* Parent;
	float Scale;  // 실제 Offset 에 영향을 줌

	// Parent == nullptr 인 경우 Offset 이 곧 Location 이 됨
	// Parent != nullptr 인 경우 Parent 로부터의 상대적 거리를 나타내는 Offset (Scale = 1 기준)
	FVector Offset;
};

/*
 * UTextWidget Render 후 UImageWidget Render 가 제대로 작동하지 않거나
 * Parent-Child 위치 관계가 보존이 안되는 등의 문제가 있어 현재로썬 단일 객체 + Rendering 순서
 */
class UTextWidget : public UWidget
{
public:
	UTextWidget() :
		TextColor(DirectX::Colors::White)
	{
	}

	void Render(URenderer& renderer) override
	{
		/*
			UI
		*/
		DirectX::SimpleMath::Vector2 textSize = renderer.Font->MeasureString(Text.c_str());

		renderer.SpriteBatch->Begin();

		// Screen 좌표 계산
		FVector ScreenCoord = CartesianToScreenCoord(GetLocation(true));

		// DrawString 중앙 정렬: Origin = 텍스트 중앙
		DirectX::XMFLOAT2 origin = { textSize.x / 2.f, textSize.y / 2.f };


		renderer.Font->DrawString(
			renderer.SpriteBatch.get(),
			Text.c_str(),
			DirectX::XMFLOAT2(ScreenCoord.x, ScreenCoord.y),
			TextColor,
			0.0f,
			origin,                        // 텍스트 중앙 기준
			GetScale() * GetParentScale()
		);

		/*
		renderer.Font->DrawString(
			renderer.SpriteBatch.get(),
			Text.c_str(),
			DirectX::XMFLOAT2(ScreenCoord.x, ScreenCoord.y),
			DirectX::Colors::White,
			0.0f,
			DirectX::XMFLOAT2(0, 0),                        // 텍스트 중앙 기준
			GetScale() * GetParentScale()
		);
		*/

		renderer.SpriteBatch->End();
	}

	void SetNumber(int Num)
	{
		Text = std::to_wstring(Num);
	}

	void SetText(const wchar_t* InText)
	{
		Text = InText;
	}

	void SetTextColor(DirectX::XMVECTORF32 InTextColor)
	{
		TextColor = InTextColor;
	}

private:
	std::wstring Text;

	DirectX::XMVECTORF32 TextColor;
};

class UImageWidget : public UWidget
{
public:
	UImageWidget() :
		Texture(nullptr),
		AxisScale(FVector::Splat(1)),
		TintColor(FVector(1, 1, 1))
	{
	}

	void Render(URenderer& renderer) override
	{
		FVector Location = GetLocation(true);
		renderer.UpdateConstant(Location, 0.f, AxisScale * GetScale() * GetParentScale(), 1.0f, TintColor);
		renderer.UpdateTexture(Texture);
		renderer.RenderRectangle();
	}

	void SetTexture(UTexture* InTexture) { Texture = InTexture; }
	void SetAxisScale(FVector InAxisScale) { AxisScale = InAxisScale; }
	void SetTintColor(FVector InTintColor) { TintColor = InTintColor; }
	FVector GetTintColor() { return TintColor; }

protected:
	UTexture* Texture;
	FVector AxisScale;
	FVector TintColor;
};

/*
 * 이미지가 없는 버튼도 있겠지만 이 게임 프로토타입에선 배제하여 이미지를 상속받음
 * 버튼의 가운데에 해당 텍스트를 넣는다면 렌더 순서가 꼬여 소유하지 않고 일단 외부 텍스트를 받도록 함
 */
class UButtonWidget : public UImageWidget
{
public:
	UButtonWidget() = default;

public:
	bool IsPressed(HWND hWnd, const UInputManager& InputManager, USoundManager& soundManger)
	{
		if (IsOnBound(hWnd) && InputManager.IsMouseReleased())
		{
			soundManger.PlaySFX(TEXT("./Sound/SFX/Click.wav"));
			return true;
		}
		return false;
	}

public:
	/*
	* 스크린 상에서 버튼 클릭 체크
	*/
	bool IsOnBound(HWND hWnd)
	{
		//NDC 좌표
		POINT lpPoint = {};
		GetCursorPos(&lpPoint);
		ScreenToClient(hWnd, &lpPoint);

		float MouseX = lpPoint.x;
		float MouseY = lpPoint.y;

		FVector CartesianCoord = GetLocation(true);

		FVector ScaledAxisScale = AxisScale * GetScale() * GetParentScale();
		FVector LeftTop = { CartesianCoord.x - ScaledAxisScale.x, CartesianCoord.y + ScaledAxisScale.y, 0.f };
		FVector RightBottom = { CartesianCoord.x + ScaledAxisScale.x, CartesianCoord.y - ScaledAxisScale.y, 0.f };

		LeftTop = { (LeftTop.x + 1.0f) * 0.5f * 1024, (1.f - LeftTop.y) * 0.5f * 1024 , 0 };
		RightBottom = { (RightBottom.x + 1.0f) * 0.5f * 1024,  (1.f - RightBottom.y) * 0.5f * 1024 , 0 };

		return (MouseX > LeftTop.x && MouseX < RightBottom.x && MouseY > LeftTop.y && MouseY < RightBottom.y);

	}
};

struct FPendingMerge
{
	EFruitType ResultFruitType;
	FVector NewLocation;
};

enum EGameState
{
	GS_Title,
	GS_Credits,
	GS_Playing,
	GS_GameOver,
	GS_HowToPlay
};
EGameState GameState = GS_Title;

static void UpdateTitle(URenderer& renderer)
{
	// 메모리 누수가 남아있는 함수입니다. 대체 예정이라 여기는 생략했습니다.

	static ID3D11ShaderResourceView* TitleBackgroundSRV = nullptr;

	if (TitleBackgroundSRV == nullptr)
	{
		UTexture* TitleBackgroundTexture = new UTexture();
		TitleBackgroundTexture->Initialize(renderer.Device, renderer.DeviceContext, "./Resource/title_bg.png");
		TitleBackgroundSRV = TitleBackgroundTexture->GetTextureSRV();
	}

	ImGui_ImplDX11_NewFrame(); // 렌더러 쪽 준비
	ImGui_ImplWin32_NewFrame(); // 플랫폼 쪽 준비
	ImGui::NewFrame(); // ImGui Core 초기화

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

	ImGui::Begin(
		"Jungle Scene Window",
		nullptr,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoTitleBar
	);

	ImDrawList* drawList = ImGui::GetWindowDrawList();

	ImVec2 pos = ImGui::GetWindowPos();
	ImVec2 size = ImGui::GetWindowSize();

	drawList->AddImage(
		(ImTextureID)TitleBackgroundSRV,
		pos,
		ImVec2(pos.x + size.x, pos.y + size.y)
	);

	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));

	const char* StartButtonText = "START";
	ImVec2 buttonSize = ImGui::CalcTextSize(StartButtonText);

	ImGui::SetCursorPos(ImVec2((ImGui::GetIO().DisplaySize.x - buttonSize.x - 20) * 0.5f, (ImGui::GetIO().DisplaySize.y - buttonSize.y) * 0.5f));

	if (ImGui::Button(StartButtonText))
	{
		GameState = GS_Playing;

		TitleBackgroundSRV->Release();
		TitleBackgroundSRV = nullptr;
	}

	const char* CreditsButtonText = "CREDITS";
	buttonSize = ImGui::CalcTextSize(CreditsButtonText);

	ImGui::SetCursorPosX((ImGui::GetIO().DisplaySize.x - buttonSize.x - 20) * 0.5f);
	if (ImGui::Button(CreditsButtonText))
	{
		GameState = GS_Credits;
	}

	ImGui::PopStyleColor();

	ImGui::End();
	ImGui::Render(); // 이번 프레임에 그릴 UI 설정
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); // DX11 백 엔드를 통해 실제 GPU 드로우 호출
}

static void UpdateCredits(URenderer& renderer)
{
	// 메모리 누수가 남아있는 함수입니다. 대체 예정이라 여기는 생략했습니다.

	static ID3D11ShaderResourceView* CreditsBackgroundSRV = nullptr;

	if (CreditsBackgroundSRV == nullptr)
	{
		UTexture* CreditsBackgroundTexture = new UTexture();
		CreditsBackgroundTexture->Initialize(renderer.Device, renderer.DeviceContext, "./Resource/credits_bg.png");
		CreditsBackgroundSRV = CreditsBackgroundTexture->GetTextureSRV();
	}

	ImGui_ImplDX11_NewFrame(); // 렌더러 쪽 준비
	ImGui_ImplWin32_NewFrame(); // 플랫폼 쪽 준비
	ImGui::NewFrame(); // ImGui Core 초기화

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

	ImGui::Begin(
		"Jungle Scene Window",
		nullptr,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoTitleBar
	);

	ImDrawList* drawList = ImGui::GetWindowDrawList();

	ImVec2 pos = ImGui::GetWindowPos();
	ImVec2 size = ImGui::GetWindowSize();

	drawList->AddImage(
		(ImTextureID)CreditsBackgroundSRV,
		pos,
		ImVec2(pos.x + size.x, pos.y + size.y)
	);

	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));

	const char* text = "Amazing Developers!!!";
	ImVec2 textSize = ImGui::CalcTextSize(text);

	float centerX = (ImGui::GetIO().DisplaySize.x - textSize.x) * 0.5f;
	float centerY = (ImGui::GetIO().DisplaySize.y - textSize.y) * 0.5f;

	ImGui::SetCursorPos(ImVec2(centerX, centerY));
	ImGui::Text("%s", text);

	textSize = ImGui::CalcTextSize("KJS");
	centerX = (ImGui::GetIO().DisplaySize.x - textSize.x) * 0.5f;
	ImGui::SetCursorPosX(centerX);
	ImGui::Text("KJS");
	ImGui::SetCursorPosX(centerX);
	ImGui::Text("PJH");
	ImGui::SetCursorPosX(centerX);
	ImGui::Text("JMJ");
	ImGui::SetCursorPosX(centerX);
	ImGui::Text("JJY");

	ImGui::Spacing();

	const char* buttonText = "BACK";
	ImVec2 buttonSize = ImGui::CalcTextSize(buttonText);

	ImGui::SetCursorPosX((ImGui::GetIO().DisplaySize.x - buttonSize.x - 20) * 0.5f);

	if (ImGui::Button(buttonText))
	{
		GameState = GS_Title;

		CreditsBackgroundSRV->Release();
		CreditsBackgroundSRV = nullptr;
	}

	ImGui::PopStyleColor();

	ImGui::End();

	ImGui::Render(); // 이번 프레임에 그릴 UI 설정
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); // DX11 백 엔드를 통해 실제 GPU 드로우 호출
}

static void UpdateGameOver(URenderer& renderer, UScoreManager& ScoreManager)
{
	// 메모리 누수가 남아있는 함수입니다. 대체 예정이라 여기는 생략했습니다.

	static ID3D11ShaderResourceView* GameOverBackgroundSRV = nullptr;

	if (GameOverBackgroundSRV == nullptr)
	{
		UTexture* GameOverBackgroundTexture = new UTexture();
		GameOverBackgroundTexture->Initialize(renderer.Device, renderer.DeviceContext, "./Resource/gameover_bg.png");
		GameOverBackgroundSRV = GameOverBackgroundTexture->GetTextureSRV();
	}

	ImGui_ImplDX11_NewFrame(); // 렌더러 쪽 준비
	ImGui_ImplWin32_NewFrame(); // 플랫폼 쪽 준비
	ImGui::NewFrame(); // ImGui Core 초기화

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

	ImGui::Begin(
		"Jungle Scene Window",
		nullptr,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoTitleBar
	);

	ImDrawList* drawList = ImGui::GetWindowDrawList();

	ImVec2 pos = ImGui::GetWindowPos();
	ImVec2 size = ImGui::GetWindowSize();

	drawList->AddImage(
		(ImTextureID)GameOverBackgroundSRV,
		pos,
		ImVec2(pos.x + size.x, pos.y + size.y)
	);

	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));

	const char* text = "Game Over";
	ImVec2 textSize = ImGui::CalcTextSize(text);

	float centerX = (ImGui::GetIO().DisplaySize.x - textSize.x) * 0.5f;
	float centerY = (ImGui::GetIO().DisplaySize.y - textSize.y) * 0.5f;

	ImGui::SetCursorPos(ImVec2(centerX, centerY));
	ImGui::Text("%s", text);

	ImGui::SetCursorPosX(centerX);
	ImGui::Text("Score: %d", ScoreManager.GetScore());

	ImGui::Spacing();

	const char* buttonText = "Restart";
	ImVec2 buttonSize = ImGui::CalcTextSize(buttonText);

	ImGui::SetCursorPosX((ImGui::GetIO().DisplaySize.x - buttonSize.x - 20) * 0.5f);

	if (ImGui::Button(buttonText))
	{
		//Reset();
		ScoreManager.UpdateHighScore();
		ScoreManager.Reset();

		GameState = GS_Playing;

		GameOverBackgroundSRV->Release();
		GameOverBackgroundSRV = nullptr;
	}

	ImGui::PopStyleColor();

	ImGui::End();

	ImGui::Render(); // 이번 프레임에 그릴 UI 설정
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); // DX11 백 엔드를 통해 실제 GPU 드로우 호출
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static inline EFruitType GetRandomFruitType()
{
	return static_cast<EFruitType>(rand() % (EFruitType::FT_Orange + 1));
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	std::srand(std::time(0));

	WCHAR WindowClass[] = L"GTLWindowClass";

	WCHAR Title[] = L"Game Tech Lab";

	WNDCLASSW wndclass = {
		0,
		WndProc,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		WindowClass
	};

	RegisterClassW(&wndclass);

	RECT rect = { 0, 0, 1024, 1024 };
	AdjustWindowRect(&rect, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW, FALSE);

	HWND hWnd = CreateWindowExW(0,
		WindowClass,
		Title,
		WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rect.right - rect.left,   // 타이틀바 포함한 전체 크기
		rect.bottom - rect.top,
		nullptr,
		nullptr,
		hInstance,
		nullptr
	);

	URenderer renderer;
	renderer.Create(hWnd); // 현재는 공 모델만 사용하기 때문에 정점 버퍼는 Create() 내에서 한 번 생성
	renderer.CreateShader();
	renderer.CreateConstantBuffer();

	const int targetFPS = 60;
	const double targetFrameTime = 1000.0 / targetFPS;
	const float fixedDeltaTime = (float)targetFrameTime / 1000; // 60fps 고정 상황 가정

	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);

	LARGE_INTEGER startTime, endTime, lastBlinkTime;
	double elapsedTime = 0.0;

	static bool BlinkInitialized = false;

	if (!BlinkInitialized)
	{
		QueryPerformanceCounter(&lastBlinkTime);
		BlinkInitialized = true;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui_ImplWin32_Init((void*)hWnd); // OS 이벤트 및 입력 담당
	ImGui_ImplDX11_Init(renderer.Device, renderer.DeviceContext); // GPU 렌더링 담당

	UScoreManager ScoreManager;
	UInputManager InputManager;
	USoundManager SoundManager;
	UParticlePool ParticlePool;

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


	TArray<UPrimitive*> PrimitiveList;

	bool bIsExit = false;

	// 이전 프레임의 선 충돌 여부
	bool prevCollision = false;

	UFruit previewFruit;

	LARGE_INTEGER collisionStartCounter;

	static const char* FruitNames[] = {
		"Cherry", "Strawberry", "Grape", "Dekopon", "Orange",
		"Apple", "Pear", "Peach", "Pineapple", "Melon", "Watermelon"
	};

	TArray<UTexture*> FruitTextures;
	for (int i = 0; i < ARRAYSIZE(FruitNames); i++)
	{
		std::string FruitPath = "./Resource/";
		FruitPath += FruitNames[i];
		FruitPath += ".png";
		FruitTextures.PushBack(new UTexture(renderer.Device, renderer.DeviceContext, FruitPath.c_str()));
	}

	// 여기 데이터 해제 필요함!!!
	UTexture* EmptyTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/preview_empty.png");
	UTexture* BubbleTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/bubble.png");
	UTexture* ButtonBGTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/WoodButton_Normal.png");
	UTexture* ButtonBGHoverTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/WoodButton_Hover.png");
	UTexture* NextTextTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/NextText.png");
	UTexture* ScoreTextTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/ScoreText.png");
	UTexture* Map1Texture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/Map1.png");
	UTexture* Map1RoofTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/Map1_Roof.png");
	UTexture* TitleBackgroundTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/title_bg.png");
	UTexture* RuleTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/rule_bg.png");
	UTexture* CreditTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/credits.png");
	UTexture* GameOverTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/gameover_bg.png");

	UImageWidget ScoreWidget, TestWidget, NextTextWidget;
	UTextWidget TestTextWidget;

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

	UImageWidget MapWidget;

	MapWidget.SetTexture(Map1Texture);
	MapWidget.SetAxisScale(FVector::Splat(1));
	MapWidget.SetOffset(FVector(0, -0.15, 0));

	UImageWidget MapRoofWidget;

	MapRoofWidget.SetTexture(Map1RoofTexture);
	MapRoofWidget.SetAxisScale(FVector(1, 0.1, 0));
	MapRoofWidget.SetOffset(FVector(0, 0.6, 0));

	UTexture* LeaderBoardTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/leaderboard.png");
	UTexture* RankBgTexture = new UTexture(renderer.Device, renderer.DeviceContext, "./Resource/rankbg.png");

	UImageWidget LeaderBoardWidget;
	UImageWidget* RankBg = new UImageWidget[RANK_NUM];
	UTextWidget* RankNum = new UTextWidget[RANK_NUM];
	UTextWidget* RankScore = new UTextWidget[RANK_NUM];

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

		RankScore[i].SetScale(1);
		RankScore[i].SetParent(&RankBg[i]);
		RankScore[i].SetOffset(FVector(0, 0.015, 0));
	}
	float spawnX = 0;
	float VibrationTime = 0.f;
	const float MaxVibrationTime = 0.3f;
	bool bIsVibrating = false;

	//왼쪽 점수
	UImageWidget LeftScoreWidget, ScoreTextWidget;
	UTextWidget  CurrentScoreWidget;

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

	// 게임 오버 점수 UI
	UImageWidget GameOverScoreImage, GameOverScoreTextImage;
	UTextWidget  GameOverScoreText;

	GameOverScoreImage.SetOffset(FVector(0.f, -0.2f, 0.f));
	GameOverScoreImage.SetTexture(ButtonBGTexture);
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


	UButtonWidget StartButton, CreditButton, RuleButton, BackButton, RestartButton, BackToTitleButton;
	UTextWidget StartText, CreditText, RuleText, BackText, RestartText, BackToTitleText;
	UImageWidget StartScreenImage, HowToPlayImage, CreditImage, GameOverImage;

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
	StartButton.SetAxisScale({ 0.2, 0.1f, 1.f });

	StartText.SetParent(&StartButton);
	StartText.SetScale(1);
	StartText.SetText(L"Game Start");

	CreditButton.SetParent(&StartButton);
	CreditButton.SetTexture(ButtonBGTexture);
	CreditButton.SetAxisScale({ 0.2, 0.1, 1.f });
	CreditButton.SetOffset({ 0.f, -0.3f, 0.f });

	CreditText.SetParent(&CreditButton);
	CreditText.SetScale(1);
	CreditText.SetText(L"Credits");

	RuleButton.SetParent(&StartButton);
	RuleButton.SetTexture(ButtonBGTexture);
	RuleButton.SetAxisScale({ 0.2, 0.1, 1.f });
	RuleButton.SetOffset({ 0.f, -0.6f, 0.f });

	RuleText.SetParent(&RuleButton);
	RuleText.SetScale(1);
	RuleText.SetText(L"How To Play");

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

	BackButton.SetOffset(FVector(-0.8f, 0.8f, 0.f));
	BackButton.SetTexture(ButtonBGTexture);
	BackButton.SetAxisScale({ 0.1, 0.05f, 1.f });

	BackText.SetParent(&BackButton);
	BackText.SetOffset({ 0, 0, 0 });
	BackText.SetScale(1);
	BackText.SetText(L"Back");

	const float SpawnCooldown = 0.5f; // 과일 생성 간 최소 시간 간격 (초)
	LARGE_INTEGER LastSpawnTime;
	QueryPerformanceCounter(&LastSpawnTime);

	// GameState 변화 간 Index 를 초기화해줘야 하는데 지금 구조 상으로 아직 안돼있어서 나중에 고쳐야할듯함
	int XPadButtonIndex = 0;

	while (bIsExit == false)
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
				bIsExit = true;
				break;
			}
		}

		InputManager.Update(hWnd, fixedDeltaTime);

		// 임시 게임 상태 저장
		EGameState FrameState = GameState;

		switch (FrameState)
		{
		case GS_Title:
			renderer.Prepare();
			renderer.PrepareShader();

			if (InputManager.IsXPadConnected())
			{
				if (InputManager.IsXPadHoldingRepeatTriggered())
				{
					if (InputManager.GetXPadStickY() < 0)
					{
						XPadButtonIndex = (XPadButtonIndex + 1) % 3;
					}
					else
					{
						if (XPadButtonIndex == 0) XPadButtonIndex = 2;
						else XPadButtonIndex -= 1;
					}
				}

				// 모든 버튼 Hover -> Normal
				// 비효율적이긴한데 시간이 없어서 우선 이렇게 처리
				StartButton.SetTexture(ButtonBGTexture);
				CreditButton.SetTexture(ButtonBGTexture);
				RuleButton.SetTexture(ButtonBGTexture);

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
			}

			StartScreenImage.Render(renderer);
			StartButton.Render(renderer);
			CreditButton.Render(renderer);
			RuleButton.Render(renderer);

			StartText.Render(renderer);
			CreditText.Render(renderer);
			RuleText.Render(renderer);

			break;

		case GS_Credits:
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

			break;

		case GS_HowToPlay:
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

			break;
		case GS_Playing:
		{
			static bool prevMouseDown = false;

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

			bool bLineCollision = false;

			static int CurrentFruitType = GetRandomFruitType();
			static int NextFruitType = GetRandomFruitType();

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
			TestWidget.SetTexture(FruitTextures[NextFruitType]);
			// 병합 예정 정보를 담은 큐
			TQueue<FPendingMerge> PendingMergeQueue;

			for (UINT i = 0; i < PrimitiveList.Num(); i++)
			{
				PrimitiveList[i]->UpdatePhysics(fixedDeltaTime);
			}
			ParticlePool.Update(fixedDeltaTime);

			// 과일 간 충돌 계산 시 기준이 너무 빡빡하면 가끔씩 충돌이 감지되지 않는 경우가 있어서 약간의 여유를 둠
			const float MergeSlop = 0.005f;

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
								SoundManager.PlaySFX(TEXT("./Sound/SFX/GameWin.wav"));
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

				// 점수 획득
				ScoreManager.AddScore(MergeInfo.ResultFruitType);
			}

			CurrentScoreWidget.SetText(std::to_wstring(ScoreManager.GetScore()).c_str());

			//새 과일이 생성된 후 마찰 적용
			const static int overlapIteration = 100;
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

					break;
				}
			}

			prevCollision = bLineCollision;

			previewFruit.SetFruitType((EFruitType)CurrentFruitType);
			if (spawnX < ((-1) * MapBoarder) + previewFruit.Radius) { spawnX = ((-1) * MapBoarder) + previewFruit.Radius; }
			else if (spawnX > MapBoarder - previewFruit.Radius) { spawnX = MapBoarder - previewFruit.Radius; }
			previewFruit.SetLocation(FVector(spawnX, 0.9f, 0));

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

			previewFruit.Render(renderer);

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

			break;
		}
		case GS_GameOver:
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
						GameState = GS_Playing;
						ScoreManager.UpdateHighScore();
						ScoreManager.Reset();
						break;

					case 1:
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

			break;
		}

		if (FrameState == GS_Playing)
		{
			MapRoofWidget.Render(renderer);

			LeftScoreWidget.Render(renderer);
			ScoreTextWidget.Render(renderer);

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
				RankScore[i].SetNumber(ScoreManager.GetHighScores()[i]);
				RankScore[i].Render(renderer);

			}

			NextTextWidget.Render(renderer);
			CurrentScoreWidget.Render(renderer);
		}

		renderer.SwapBuffer();

		// 모니터 주사율이 144Hz일 수도 있으니 VSync를 사용하더라도 60fps 고정 코드는 유지
		do
		{
			Sleep(0);

			QueryPerformanceCounter(&endTime);

			elapsedTime = (endTime.QuadPart - startTime.QuadPart) * 1000.0 / frequency.QuadPart;
		} while (elapsedTime < targetFrameTime);
	}

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
	delete[] RankScore;

	// 텍스처 해제
	delete EmptyTexture;
	delete BubbleTexture;
	delete ButtonBGTexture;
	delete NextTextTexture;
	delete ScoreTextTexture;
	delete Map1Texture;
	delete Map1RoofTexture;


	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	renderer.ReleaseConstantBuffer();
	renderer.ReleaseShader();
	renderer.Release();

	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui::GetCurrentContext() != nullptr)
	{
		if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		{
			return true;
		}
	}

	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}
