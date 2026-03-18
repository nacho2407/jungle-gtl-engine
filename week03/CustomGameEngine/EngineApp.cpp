#include "EngineApp.h"
#include "Component/CameraComponent.h"
#include "Component/AxisComponent.h"
#include "Component/CubeComponent.h"
#include "Component/GizmoComponent.h"
#include "Renderer/Renderer.h"
#include "Editor/Editor.h"
#include "Editor/Gizmo.h"
#include "Editor/Picker.h"
#include "ImGui/imgui.h"
#include "InputManager.h"
#include "Logger.h"
#include "Object.h"
#include "ObjectFactory.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "TimerManager.h"
#include "World.h"


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
	D3D11_INPUT_ELEMENT_DESC layout[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
	 D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
	 D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	ResourceManager::GetInstance()->CreateMaterial(L"Shader/ShaderW0.hlsl", L"Shader/ShaderW0.hlsl", Renderer->Device.Get(), layout, 2);
	ResourceManager::GetInstance()->CreateMaterial(L"Shader/ShaderInfiniteGrid.hlsl", L"Shader/ShaderInfiniteGrid.hlsl", Renderer->Device.Get(), nullptr, 0);
	ResourceManager::GetInstance()->AddCubeMesh(Renderer->Device.Get());
	ResourceManager::GetInstance()->AddSphereMesh(Renderer->Device.Get());
	ResourceManager::GetInstance()->AddTriangleMesh(Renderer->Device.Get());
	ResourceManager::GetInstance()->AddPlaneMesh(Renderer->Device.Get());
	ResourceManager::GetInstance()->AddGizmoTranslationMesh(Renderer->Device.Get());
	ResourceManager::GetInstance()->AddGizmoRotationMesh(Renderer->Device.Get());
	ResourceManager::GetInstance()->AddGizmoScaleMesh(Renderer->Device.Get());
	ResourceManager::GetInstance()->AddAxisMesh(Renderer->Device.Get());

	InputManager::GetInstance().hWnd = HWnd;
	// Init Camera
	auto Camera = Cast<UCameraComponent>(GetWorld().AddPermanentSceneComponent<UCameraComponent>());
	Camera->SetRelativeLocation(FVector(-20.f, -20.f, 20));
	Camera->SetRelativeRotation(FRotator(0, 45.0f, 45.0f));
	Renderer->SetRenderCamera(Camera);
	EditorInst->SetCamera(Camera);
	EditorInst->SetRenderer(Renderer);
	
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
	GetWorld().NewScene();

	// Init Timer
	TimerManager::GetInstance().CreateGlobalTimer();

	return true;
}

void EngineApp::Run()
{
	//----TEST
	//UCubeComp* cube = GetWorld().AddSceneComponent<UCubeComp>();
	//cube->SetRelativeLocation(FVector(0.0, 0, 0));
	//cube->SetRelativeScale3D(FVector(2.f, 2.f, 2.f));

	//UCubeComp* Cube = GetWorld().AddSceneComponent<UCubeComp>();
	//Cube->SetRelativeLocation(FVector(30.f, 0.f, 0.f));
	//Cube->SetRelativeScale3D(FVector(1.f, 1.f, 1.f));

	//UCubeComp* Cube3 = GetWorld().AddSceneComponent<UCubeComp>();
	//Cube->SetRelativeLocation(FVector(-30.f, 0.f, 0.f));
	//Cube->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
	//-----TEST

	//UAxisComp* mainAxis = GetWorld().AddSceneComponent<UAxisComp>();

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
		float TempDelta = globalTimer->GetDeltaTime();

		ImGuiIO& io = ImGui::GetIO();

		FVector Mouse = InputManager::GetInstance().MousePos;
		if (!io.WantCaptureMouse && InputManager::GetInstance().IsKeyDown(VK_LBUTTON))
		{
			Picker->Pick(Mouse.x, Mouse.y, GetWorld().GetActiveScene());
		}

		// Update Game World
		GetWorld().Update(TempDelta);

		//Render
		GetWorld().OnBeforeRender();

		Renderer->Prepare();

		ResourceManager* RM = ResourceManager::GetInstance();
		Renderer->RenderGrid(RM->GetMaterial(L"Shader/ShaderInfiniteGrid.hlsl").VertexShader.Get(), RM->GetMaterial(L"Shader/ShaderInfiniteGrid.hlsl").PixelShader.Get());

		Renderer->Render();
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
