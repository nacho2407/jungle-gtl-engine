#include "EngineApp.h"
#include "Component/CameraComponent.h"
#include "Component/AxisComponent.h"
#include "Component/BillboardSubUVComponent.h"
#include "Component/CubeComponent.h"
#include "Component/GizmoComponent.h"
#include "Renderer/Renderer.h"
#include "Editor/Editor.h"
#include "Editor/Gizmo.h"
#include "Editor/Picker.h"
#include "ImGui/imgui.h"
#include "EngineCore/InputManager.h"
#include "EngineCore/Logger.h"
#include "Component/Object.h"
#include "EngineCore/ObjectFactory.h"
#include "EngineCore/ResourceManager.h"
#include "Scene.h"
#include "EngineCore/TimerManager.h"
#include "Component/GridComp.h"
#include "Component/AxisComponent.h"
#include "World.h"
#include "LineBatcher.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK EngineApp::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		return true;

	EngineApp* pApp = reinterpret_cast<EngineApp*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_NCCREATE: // 창이 처음 생성될 때 호출됨
	{
		// CreateWindowExW의 마지막 인자(this)를 꺼냅니다.
		LPCREATESTRUCT pcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
		pApp = reinterpret_cast<EngineApp*>(pcs->lpCreateParams);
		// 이를 윈도우 데이터에 저장합니다.
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pApp));
		return 1;
	}
	break;
	case WM_SIZE:
		if (pApp)
		{
			UINT width = LOWORD(lParam);
			UINT height = HIWORD(lParam);
			if (width > 0 && height > 0)
			{
				pApp->ProcessOnResizeCallback(width, height);
			}
		}
		break;
	case WM_SETFOCUS:
		InputManager::GetInstance().hWnd = (hWnd);
		break;

	case WM_KILLFOCUS:
		InputManager::GetInstance().hWnd = NULL;
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

EngineApp::EngineApp()
{
	Renderer = new URenderer();
	EditorInst = new Editor();
}

EngineApp::~EngineApp()
{
	delete Picker;
	delete EditorInst;
	delete Renderer;
}

bool EngineApp::Initialize(HINSTANCE hInstance)
{
	CreateEngineWindow();
	Renderer->Create(HWnd);
	EditorInst->Initialize(HWnd, Renderer->Device.Get(), Renderer->DeviceContext.Get());

	// Init ResoureManager
	ResourceManager::GetInstance()->Initialize(Renderer);

	InputManager::GetInstance().hWnd = HWnd;
	// Init Camera
	auto Camera = Cast<UCameraComponent>(GetWorld().GetSceneManager()->AddPermanentSceneComponent<UCameraComponent>());
	auto Grid = Cast<UGridComp>(GetWorld().GetSceneManager()->AddPermanentSceneComponent<UGridComp>());
	auto Axis = Cast<UAxisComp>(GetWorld().GetSceneManager()->AddPermanentSceneComponent<UAxisComp>());
	Camera->SetRelativeLocation(FVector(-20.f, -20.f, 20));
	Camera->SetRelativeRotation(FRotator(0, 45.0f, 45.0f));
	Renderer->SetRenderCamera(Camera);
	EditorInst->SetCamera(Camera);
	EditorInst->SetRenderer(Renderer);
	EditorInst->SetGrid(Grid);
	EditorInst->LoadSettings();


	Gizmo::GetInstance().CreateGizmoController();


	//Resize Callback
	OnResizeCallback.push_back([Camera](int width, int height) {
		Camera->OnResize(width, height);
		});
	OnResizeCallback.push_back([this](int width, int height) {
		Renderer->OnResize(width, height);
		});
	RECT Rect;
	if (GetClientRect(HWnd, &Rect))
	{
		ProcessOnResizeCallback(Rect.right - Rect.left, Rect.bottom - Rect.top);
	}

	//Init Picker
	Picker = Cast<UPicker>(FObjectFactory::ConstructObject(UPicker::GetClass()));
	Picker->SetEditor(EditorInst);
	Picker->SetCamera(Camera);

	//Init World & Scene
	GetWorld().InjectRenderer(Renderer);
	LastActiveScene = GetWorld().GetSceneManager()->NewScene();

	/*SubUV를 테스트를 위한 코드입니다.*/
	auto SubUVBillboard = Cast<UBillboardSubUVComponent>(GetWorld().GetSceneManager()->AddSceneComponent<UBillboardSubUVComponent>(LastActiveScene));
	if (SubUVBillboard != nullptr)
	{
		SubUVBillboard->SetRelativeLocation(FVector(0.0f, 0.0f, 3.0f));
		//SetGrid는 SubUV의 크기에 따라 나눕니다. 분할은 UV에 따라 다를 수 있으니 하드코딩을 피하기 위해 수정이 필요합니다.
		SubUVBillboard->SetGrid(6, 6, 36);
		//속도 조절인데 1초에 몇번 돌아가는지를 명시합니다. 36이면 1초마다 순회를 합니다.
		SubUVBillboard->SetPlayback(16.0f, true);
		//크기입니다.
		SubUVBillboard->SetSize(4.0f, 4.0f);
	}
	/*여기까지가 테스트 코드입니다.*/

	// Init Timer
	TimerManager::GetInstance().CreateGlobalTimer();

	return true;
}

void EngineApp::Run()
{
	Timer* globalTimer = TimerManager::GetInstance().GetGlobalTimer();
	globalTimer->Reset();

	bool bIsExit = false;
	while (bIsExit == false)
	{
		MSG msg;
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

		// Check for scene change
		UScene* currentActiveScene = GetWorld().GetSceneManager()->GetActiveScene();
		if (currentActiveScene != LastActiveScene)
		{
			EditorInst->ClearSelection();
			LastActiveScene = currentActiveScene;
		}

		TimerManager::GetInstance().Tick();
		InputManager::GetInstance().Update();
		float TempDelta = globalTimer->GetDeltaTime();

		ImGuiIO& io = ImGui::GetIO();

		//마우스 입력에 따라 picking 처리. 컨트롤 키가 눌렸다면 멀티 pick을 시도합니다.
		FVector Mouse = InputManager::GetInstance().MousePos;
		if (!io.WantCaptureMouse && InputManager::GetInstance().IsKeyDown(VK_LBUTTON))
		{
			bool bIsCtrlDown = InputManager::GetInstance().IsKeyHold(VK_CONTROL);
			UScene* activeScene = GetWorld().GetSceneManager()->GetActiveScene();
			if (activeScene != nullptr)
			{
				Picker->Pick(Mouse.x, Mouse.y, activeScene, bIsCtrlDown);
			}
		}

		// Update Game World
		GetWorld().Update(TempDelta);

		//Render
		GetWorld().OnBeforeRender();
		Renderer->Prepare();


		SceneManager* SceneMgr = GetWorld().GetSceneManager();
		TArray<FLevel*> LevelsToRender;
		if (SceneMgr->GetPermanentScene())
		{
			LevelsToRender.push_back(&SceneMgr->GetPermanentScene()->Level);
		}
		if (SceneMgr->GetActiveScene())
		{
			LevelsToRender.push_back(&SceneMgr->GetActiveScene()->Level);
		}
		GetWorld().DrawDebugLines();
		Renderer->Render(LevelsToRender);
		Renderer->BatchRenderDebugLine(FLineBatcher::GetInstance().GetLineVertices());
		FLineBatcher::GetInstance().ClearLines();
		EditorInst->DrawUI();

		Renderer->SwapBuffer();
	}
}

void EngineApp::Finalize()
{
	if (EditorInst)
	{
		EditorInst->Finalize();
	}

	if (Renderer)
	{
		Renderer->Release();
	}

	if (HWnd)
	{
		ReleaseEngineWindow();
	}
	ResourceManager::GetInstance()->Release();
}

void EngineApp::CreateEngineWindow()
{
	WCHAR windowClass[] = L"JungleWindowClass";
	WCHAR title[] = L"Custom Engine";
	WNDCLASSW wndClass = { 0, WndProc, 0, 0, 0, 0, 0, 0, 0, windowClass };

	RegisterClass(&wndClass);

	HWnd = CreateWindowExW(0, windowClass, title,
		WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 1920, 1080, nullptr,
		nullptr, nullptr, this);//lpParam에 this 넣어서 WndProc에서 접근 가능하도록
}

void EngineApp::ReleaseEngineWindow()
{
	if (HWnd)
	{
		DestroyWindow(HWnd);
		HWnd = nullptr;
	}
}

void EngineApp::ProcessOnResizeCallback(int width, int height)
{
	for (auto& callback : OnResizeCallback)
	{
		callback(width, height);
	}
}
