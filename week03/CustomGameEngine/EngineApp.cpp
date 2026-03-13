#include "EngineApp.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "FObjectFactory.h"
#include "UCubeComp.h"
#include "Editor.h"
#include "Logger.h"
#include "UCamera.h"
#include "InputManager.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK EngineApp::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		return true;

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
	// Create Shader
	D3D11_INPUT_ELEMENT_DESC layout[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
	 D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
	 D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	ResourceManager::GetInstance()->CompileVertexShaderAndLayout(L"ShaderW0.hlsl", "mainVS", Renderer->Device, layout, 2);
	ResourceManager::GetInstance()->CompilePixelShader(L"ShaderW0.hlsl", "mainPS", Renderer->Device);
	ResourceManager::GetInstance()->AddCubeMesh(Renderer->Device);

	return true;
}

void EngineApp::Run()
{
	//----TEST
	UObject* sceneobj = FObjectFactory::ConstructObject(UScene::GetClass());
	UScene* Scene = dynamic_cast<UScene*>(sceneobj);
	Scene->InjectRenderer(Renderer);


	UObject* cubeobj = FObjectFactory::ConstructObject(UCubeComp::GetClass());
	UCubeComp* Cube = dynamic_cast<UCubeComp*>(cubeobj);
	Scene->PrimitiveContainer.push_back(Cube);
	UObject* cubeobj2 = FObjectFactory::ConstructObject(UCubeComp::GetClass());
	UCubeComp* Cube2 = dynamic_cast<UCubeComp*>(cubeobj2);
	Cube2->RelativeLocation = FVector(0.5, 0, 0);
	Cube2->RelativeScale3D = FVector(0.5, 0.5, 0.5);
	Scene->PrimitiveContainer.push_back(Cube2);
	//-----TEST

	UCamera Camera;
        Camera.SetPosition(FVector(0.f, 0.f, -5.f));
	Camera.OnResize(1024, 1024);

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
        if (InputManager::GetInstance().IsKeyHold(VK_UP) &&
            !InputManager::GetInstance().IsKeyHold(VK_DOWN)) {
            Camera.MoveUp();
        } else if (!InputManager::GetInstance().IsKeyHold(VK_UP) &&
                    InputManager::GetInstance().IsKeyHold(VK_DOWN)) {
            Camera.MoveDown();
        }
        if (InputManager::GetInstance().IsKeyHold(VK_LEFT) &&
            !InputManager::GetInstance().IsKeyHold(VK_RIGHT)) {
            Camera.MoveLeft();
        } else if (!InputManager::GetInstance().IsKeyHold(VK_LEFT) &&
                    InputManager::GetInstance().IsKeyHold(VK_RIGHT)) {
            Camera.MoveRight();
        }
		Camera.update();


		// Main Loop
		Renderer->Prepare();
		Renderer->PrepareShader();

		Scene->Update(0.016f);
		Scene->Render();

		Renderer->Render(&Camera);

		Renderer->SwapBuffer();
	}
}

void EngineApp::Finalize()
{
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
		nullptr, nullptr, nullptr);
}

void EngineApp::ReleaseEngineWindow()
{
	if (HWnd)
	{
		DestroyWindow(HWnd);
		HWnd = nullptr;
	}
}
