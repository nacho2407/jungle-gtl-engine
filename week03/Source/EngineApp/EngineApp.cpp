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
	case WM_CLOSE:
		DestroyWindow(hWnd);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
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
	case WM_ENTERSIZEMOVE:
		if (pApp)
		{
			pApp->bIsResizing = true;
			SetTimer(hWnd, ResizeRenderTimerId, 16, nullptr);
		}
		return 0;
	case WM_EXITSIZEMOVE:
		if (pApp)
		{
			KillTimer(hWnd, ResizeRenderTimerId);
			pApp->bIsResizing = false;
			pApp->RenderCurrentFrame();
		}
		return 0;
	case WM_TIMER:
		if (pApp && pApp->bIsResizing && wParam == ResizeRenderTimerId)
		{
			// 리사이즈 중에는 타이머 기반으로 업데이트/렌더 유지함
			pApp->RenderResizeFrame();
			return 0;
		}
		break;
	case WM_SETFOCUS:
		InputManager::GetInstance().hWnd = (hWnd);
		break;

	case WM_KILLFOCUS:
		InputManager::GetInstance().hWnd = NULL;
		break;

	case WM_MOUSEWHEEL:
		InputManager::GetInstance().AccumulateMouseWheelDelta((float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA);
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
	Picker.SetEditor(EditorInst);
	Picker.SetCamera(Camera);

	//Init World & Scene
	GetWorld().InjectRenderer(Renderer);
	LastActiveScene = GetWorld().GetSceneManager()->NewScene();

	// Init Timer
	TimerManager::GetInstance().CreateGlobalTimer();

	return true;
}

void EngineApp::TickGameFrame(float deltaTime, bool bAllowPicking)
{
	// 씬 전환 시 선택 상태 정리함
	UScene* currentActiveScene = GetWorld().GetSceneManager()->GetActiveScene();
	if (currentActiveScene != LastActiveScene)
	{
		EditorInst->ClearSelection();
		LastActiveScene = currentActiveScene;
	}

	TimerManager::GetInstance().Tick();
	InputManager::GetInstance().Update();

	if (bAllowPicking)
	{
		ImGuiIO& io = ImGui::GetIO();

		// 마우스 클릭 시 picking 수행함
		FVector Mouse = InputManager::GetInstance().MousePos;
		if (!io.WantCaptureMouse && InputManager::GetInstance().IsKeyDown(VK_LBUTTON))
		{
			bool bIsCtrlDown = InputManager::GetInstance().IsKeyHold(VK_CONTROL);
			UScene* activeScene = GetWorld().GetSceneManager()->GetActiveScene();
			if (activeScene != nullptr)
			{
				Picker.Pick(Mouse.x, Mouse.y, activeScene, bIsCtrlDown);
			}
		}
	}

	// 월드 업데이트 수행함
	GetWorld().Update(deltaTime);
}

void EngineApp::RenderCurrentFrame()
{
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
	Renderer->BatchRenderDebugLine(FLineBatcher::GetInstance().GetLineVertices());
	FLineBatcher::GetInstance().ClearLines();
	Renderer->Render(LevelsToRender);

	// 이제 여기서 바로 Billboard 텍스트 생성해서 그립니다!
	TArray<FBillboardTextDrawItem> BillboardTextItems;
	EditorInst->BuildSelectedBillboardTextItems(BillboardTextItems);

	Renderer->RenderBillboardTextBatch(BillboardTextItems);
	EditorInst->DrawUI();
	Renderer->SwapBuffer();
}

void EngineApp::RenderResizeFrame()
{
	if (Renderer == nullptr)
	{
		return;
	}

	Timer* globalTimer = TimerManager::GetInstance().GetGlobalTimer();
	if (globalTimer == nullptr)
	{
		return;
	}

	// 리사이즈 중에는 피킹만 막고 내부 갱신은 유지함
	TickGameFrame(globalTimer->GetDeltaTime(), false);
	RenderCurrentFrame();
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

		float TempDelta = globalTimer->GetDeltaTime();

		if (!bIsResizing)
		{
			TickGameFrame(TempDelta, true);
		}

		RenderCurrentFrame();
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
