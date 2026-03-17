#include "EngineApp.h"
#include "Camera.h"
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
#include "SceneManager.h"
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
	delete Camera;
	delete Picker;
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
	ResourceManager::CreateMaterial(L"Shader/ShaderW0.hlsl", L"Shader/ShaderW0.hlsl", Renderer->Device, layout, 2);
	ResourceManager::CreateMaterial(L"Shader/ShaderInfiniteGrid.hlsl", L"Shader/ShaderInfiniteGrid.hlsl", Renderer->Device, nullptr, 0);
	ResourceManager::AddCubeMesh(Renderer->Device);
	ResourceManager::AddSphereMesh(Renderer->Device);
	ResourceManager::AddTriangleMesh(Renderer->Device);
	ResourceManager::AddPlaneMesh(Renderer->Device);
	ResourceManager::AddGizmoTranslationMesh(Renderer->Device);
	ResourceManager::AddGizmoRotationMesh(Renderer->Device);
	ResourceManager::AddGizmoScaleMesh(Renderer->Device);
	ResourceManager::AddAxisMesh(Renderer->Device);

	InputManager::GetInstance().hWnd = HWnd;
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
	//UCubeComp* cube = GetWorld().AddSceneComponent<UCubeComp>();
	//cube->SetRelativeLocation(FVector(0.0, 0, 0));
	//cube->SetRelativeScale3D(FVector(2.f, 2.f, 2.f));

	//UCubeComp* Cube = GetWorld().AddSceneComponent<UCubeComp>();
	//Cube->SetRelativeLocation(FVector(30.f, 0.f, 0.f));
	//Cube->SetRelativeScale3D(FVector(1.f, 1.f, 1.f));

	//UCubeComp* Cube3 = GetWorld().AddSceneComponent<UCubeComp>();
	//Cube->SetRelativeLocation(FVector(-30.f, 0.f, 0.f));
	//Cube->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));

	UAxisComp* mainAxis = GetWorld().AddSceneComponent<UAxisComp>();

	Camera->SetPosition(FVector(-10, 0, 0));
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
		float TempDelta = globalTimer->GetDeltaTime();
		UScene* Scene = GetWorld().GetActiveScene();

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

		if (!io.WantCaptureKeyboard && !io.WantCaptureMouse)
		{
			if (InputManager::GetInstance().IsKeyHold(VK_RBUTTON))
			{
				Camera->Move(TempDelta);
				Camera->Rotate(TempDelta);
			}
			else if (InputManager::GetInstance().IsKeyHold(VK_LBUTTON)
				&& InputManager::GetInstance().IsKeyHold(VK_MENU))
			{
				Camera->OrbitRotate(TempDelta);
			}
			if (InputManager::GetInstance().IsKeyHold('F')
				&& GetWorld().GetActiveScene()->IsCompSelected)
			{
				Camera->SetPosition(EditorInst->GetSelectedComponent()->GetRelativeLocation() - Camera->Direction * Camera->FocusLength);
			}
		}
		Camera->update();

		//Render
		Renderer->Prepare();

		ResourceManager* RM = ResourceManager::GetInstance();
		Renderer->RenderGrid(Camera, RM->GetMaterial(L"Shader/ShaderInfiniteGrid.hlsl").VertexShader, RM->GetMaterial(L"Shader/ShaderInfiniteGrid.hlsl").PixelShader);
		
		Scene->Update(TempDelta);
		GetWorld().OnBeforeRender();
		EditorInst->OnBeforeRender();

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
