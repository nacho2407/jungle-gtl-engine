#include "EngineApp.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "FObjectFactory.h"
#include "UCubeComp.h"
#include "Editor.h"
#include "Logger.h"
#include "TimerManager.h"
#include "SceneManager.h"
#include "World.h"
#include "UCamera.h"
#include "InputManager.h"
#include "Gizmo.h"
#include "UPicker.h"
#include "World.h";
#include "ImGui/imgui.h"

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
				if (pApp->Camera)
					pApp->Camera->OnResize(width, height);

				if (pApp->Renderer)
					pApp->Renderer->OnResize(width, height);
			}
		}
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
	EditorInst->Initialize(HWnd, Renderer->Device, Renderer->DeviceContext);

	// Init ResoureManager
	D3D11_INPUT_ELEMENT_DESC layout[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
	 D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
	 D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	ResourceManager::CreateMaterial(L"ShaderW0.hlsl", L"ShaderW0.hlsl", Renderer->Device, layout, 2);
	ResourceManager::AddCubeMesh(Renderer->Device);
	ResourceManager::AddSphereMesh(Renderer->Device);
	ResourceManager::AddTriangleMesh(Renderer->Device);
	ResourceManager::AddGizmoTranslationMesh(Renderer->Device);

	// Init Camera
	Camera = Cast<UCamera>(FObjectFactory::ConstructObject(UCamera::GetClass()));
	EditorInst->SetCamera(Camera);
	EditorInst->SetRenderer(Renderer);

	RECT rc;
	int width = 0;
	int height = 0;
	if (GetClientRect(HWnd, &rc))
	{
		width = rc.right - rc.left;
		height = rc.bottom - rc.top;
	}
	Camera->OnResize(width, height);

	//Init Picker
	Picker = Cast<UPicker>(FObjectFactory::ConstructObject(UPicker::GetClass()));
	Picker->SetEditor(EditorInst);

	//Init World & Scene
	GetWorld().InjectRenderer(Renderer);
	SceneManager::GetInstance().NewScene();

	// Init Timer
	TimerManager::GetInstance().CreateGlobalTimer();

	return true;
}

void EngineApp::Run()
{
	//----TEST
	UObject* sceneobj = FObjectFactory::ConstructObject(UScene::GetClass());


	UObject* cubeobj2 = FObjectFactory::ConstructObject(UCubeComp::GetClass());
	UCubeComp* Cube2 = Cast<UCubeComp>(cubeobj2);
	Cube2->RelativeLocation = FVector(0.0, 0, 0);
	Cube2->RelativeScale3D = FVector(2.f, 2.f, 2.f);
	GetWorld().GetActiveScene()->PrimitiveContainer.push_back(Cube2);

	Camera->SetPosition(FVector(0, 0, -10.f));
	//-----TEST

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


		TimerManager::GetInstance().Tick();
		InputManager::GetInstance().Update();
		UScene* Scene = GetWorld().GetActiveScene();

		ImGuiIO& io = ImGui::GetIO();

		FVector Mouse = InputManager::GetInstance().MousePos;
		if (!io.WantCaptureMouse && InputManager::GetInstance().IsKeyDown(VK_LBUTTON))
		{
			Picker->Pick(Mouse.x, Mouse.y, Camera, GetWorld().GetActiveScene());
		}

		if (!io.WantCaptureKeyboard && !io.WantCaptureMouse)
		{
			Camera->Move();
			Camera->Rotate();
		}

		bool bMouseHold = InputManager::GetInstance().IsKeyHold(VK_LBUTTON);
		if (bMouseHold && !io.WantCaptureMouse)
		{
			Ray currentRay = Picker->CreateRay(Mouse.x, Mouse.y, Camera);

			if (Gizmo::GetInstance().IsDragging())
			{
				Gizmo::GetInstance().UpdateDrag(currentRay);
			}
		}
		else
		{
			if (Gizmo::GetInstance().IsDragging())
			{
				Gizmo::GetInstance().EndDrag();
				UE_LOG("Ended Gizmo Drag");
			}
		}

		Camera->update();

		//Render
		Renderer->Prepare();
		Scene->Update(globalTimer->GetDeltaTime());
		GetWorld().Render();
		EditorInst->Render();

		Renderer->Render(Camera);
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
}

void EngineApp::CreateEngineWindow()
{
	WCHAR windowClass[] = L"JungleWindowClass";
	WCHAR title[] = L"Custom Engine";
	WNDCLASSW wndClass = { 0, WndProc, 0, 0, 0, 0, 0, 0, 0, windowClass };

	RegisterClass(&wndClass);

	HWnd = CreateWindowExW(0, windowClass, title,
		WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 1024, 1024, nullptr,
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
