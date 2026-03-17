#include "EngineApp.h"
#include "Component/CameraComponent.h"
#include "Component/AxisComponent.h"
#include "Component/CubeComponent.h"
#include "Renderer/Renderer.h"
#include "Editor/Editor.h"
#include "Editor/Gizmo.h"
#include "Editor/GizmoController.h"
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
	Camera = Cast<UCameraComponent>(GetWorld().AddPermanentSceneComponent<UCameraComponent>());
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

	UAxisComp* mainAxis = GetWorld().AddPermanentSceneComponent<UAxisComp>();
	Camera->SetPosition(FVector(-10, 0, 0));

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
			Picker->Pick(Mouse.x, Mouse.y, Camera, GetWorld().GetActiveScene());
		}

		Ray ray = Picker->CreateRay(Mouse.x, Mouse.y, Camera);

		if (InputManager::GetInstance().IsKeyHold(VK_LBUTTON))
		{
			if (Gizmo::GetInstance().GetController()->IsDragging())
			{
				Gizmo::GetInstance().GetController()->UpdateDrag(ray);
			}
		}

		if (InputManager::GetInstance().IsKeyUp(VK_LBUTTON))
		{
			if (Gizmo::GetInstance().GetController()->IsDragging())
			{
				Gizmo::GetInstance().GetController()->EndDrag();
			}
		}

		if (!InputManager::GetInstance().IsKeyHold(VK_LBUTTON))
		{
			GizmoControllerAxis hoveredAxis = Gizmo::GetInstance().GetController()->TryPick(ray);
			Gizmo::GetInstance().GetController()->SetHoverAxis(hoveredAxis);
		}

		// Update Game World
		GetWorld().Update(TempDelta);

		//Render
		GetWorld().OnBeforeRender();
		EditorInst->OnBeforeRender();

		Renderer->Prepare();

		ResourceManager* RM = ResourceManager::GetInstance();
		Renderer->RenderGrid(Camera, RM->GetMaterial(L"Shader/ShaderInfiniteGrid.hlsl").VertexShader.Get(), RM->GetMaterial(L"Shader/ShaderInfiniteGrid.hlsl").PixelShader.Get());

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
