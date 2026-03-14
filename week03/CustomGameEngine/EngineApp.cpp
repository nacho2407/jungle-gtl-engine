#include "EngineApp.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "FObjectFactory.h"
#include "UCubeComp.h"
#include "Editor.h"
#include "Logger.h"
#include "TimerManager.h"
#include "UCamera.h"
#include "InputManager.h"

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
	ResourceManager::GetInstance()->CompileVertexShaderAndLayout(L"ShaderW0.hlsl", "mainVS", Renderer->Device, layout, 2);
	ResourceManager::GetInstance()->CompilePixelShader(L"ShaderW0.hlsl", "mainPS", Renderer->Device);
	ResourceManager::GetInstance()->AddCubeMesh(Renderer->Device);

	// Init Camera
	Camera = Cast<UCamera>(FObjectFactory::ConstructObject(UCamera::GetClass()));
	EditorInst->SetCamera(Camera);

	RECT rc;
	int width = 0;
	int height = 0;
	if (GetClientRect(HWnd, &rc))
	{
		width = rc.right - rc.left;   // 가로 크기
		height = rc.bottom - rc.top; // 세로 크기
		// 이 값을 Aspect Ratio 계산에 넣으시면 됩니다!
	}
	Camera->OnResize(width, height);

	// Init Timer
	TimerManager::GetInstance().CreateGlobalTimer();

	return true;
}

void EngineApp::Run()
{
	//----TEST
	UObject* sceneobj = FObjectFactory::ConstructObject(UScene::GetClass());
	UScene* Scene = Cast<UScene>(sceneobj);
	Scene->InjectRenderer(Renderer);


	UObject* cubeobj = FObjectFactory::ConstructObject(UCubeComp::GetClass());
	UCubeComp* Cube = Cast<UCubeComp>(cubeobj);
	Scene->PrimitiveContainer.push_back(Cube);
	UObject* cubeobj2 = FObjectFactory::ConstructObject(UCubeComp::GetClass());
	UCubeComp* Cube2 = Cast<UCubeComp>(cubeobj2);
	Cube2->RelativeLocation = FVector(0.0, 0, 0);
	Cube2->RelativeScale3D = FVector(2.f, 2.f, 2.f);
	Scene->PrimitiveContainer.push_back(Cube2);

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

		InputManager::GetInstance().Update();

		Camera->Move();
        Camera->Rotate();
		Camera->update();

		// Main Loop
		TimerManager::GetInstance().Tick();

		Renderer->Prepare();

		Scene->Update(globalTimer->GetDeltaTime());
		Scene->Render();

		Renderer->Render(Camera);
		EditorInst->Render();

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
