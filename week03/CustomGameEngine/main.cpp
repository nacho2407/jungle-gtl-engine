#include <Windows.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#include "FVector.h"
#include "FMatrix.h"
#include "URender.h"
#include "FVertexSimple.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_internal.h"
#include "Sphere.h"

static constexpr float PI{ 3.1415927f };
static constexpr float G{ -0.98f };
static constexpr float leftBorder{ -1.0f };
static constexpr float rightBorder{ 1.0f };
static constexpr float topBorder{ 1.0f };
static constexpr float bottomBorder{ -1.0f };

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg,
	WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam,
	LPARAM lParam) {
	if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam)) {
		return true;
	}

	switch (message) {
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR IpCmdLine, int nShowCmd) {
	WCHAR WindowClass[] = L"JungleWindowClass";
	WCHAR Title[] = L"Game Tech Lab";
	WNDCLASSW wndclass = { 0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };

	RegisterClassW(&wndclass);

	HWND hwnd = CreateWindowExW(0, WindowClass, Title,
		WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 1024, 1024, nullptr,
		nullptr, hInstance, nullptr);
	URenderer renderer;
	renderer.Create(hwnd);
	renderer.CreateShader();
	renderer.CreateConstantBuffer();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplWin32_Init((void*)hwnd);
	ImGui_ImplDX11_Init(renderer.Device, renderer.DeviceContext);

	FVertexSimple* vertices = sphere_vertices;
	UINT ByteWidth = sizeof(sphere_vertices);
	UINT numVerticesSphere = sizeof(sphere_vertices) / sizeof(FVertexSimple);

	ID3D11Buffer* vertexBufferSphere =
		renderer.CreateVertexBuffer(sphere_vertices, ByteWidth);

	const int targetFPS{ 60 };
	const double targetFrameTime{ 1000.0 / targetFPS };

	LARGE_INTEGER frequency;
	LARGE_INTEGER startTime, endTime;
	double elapsedTime{ 0.0 };
	QueryPerformanceFrequency(&frequency);

	bool bShowSpawnError{ false };
	float ErrorDisplayTimer{ 0.0f };
	int ImGuiBallCount{ 1 };
	bool SpinEnable{ false };
	bool GravityEnable{ false };

	bool bIsExit{ false };
	while (bIsExit == false) {
		MSG msg;
		QueryPerformanceCounter(&startTime);

		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT) {
				bIsExit = true;
				break;
			}
		}


		renderer.Prepare();
		renderer.PrepareShader();

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Jungle Property Window");
		ImGui::Text("Hello Jungle World!");
		ImGui::Checkbox("Gravity", &GravityEnable);
		ImGui::Checkbox("Spin", &SpinEnable);
		if (ImGui::InputInt("Number of Ball", &ImGuiBallCount)) {
			if (ImGuiBallCount < 1) ImGuiBallCount = 1;
			if (ImGuiBallCount > 100) ImGuiBallCount = 100;
		}
		if (bShowSpawnError) {
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
				"Error: No space to spawn more balls!");
			ErrorDisplayTimer -= (float)targetFrameTime / 1000.0f;
			if (ErrorDisplayTimer <= 0) bShowSpawnError = false;
		}
		ImGui::End();
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		renderer.SwapBuffer();

		do {
			Sleep(0);
			QueryPerformanceCounter(&endTime);
			elapsedTime =
				(endTime.QuadPart - startTime.QuadPart) * 1000.0 / frequency.QuadPart;

		} while (elapsedTime < targetFrameTime);
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	renderer.ReleaseVertexBuffer(vertexBufferSphere);

	renderer.ReleaseConstantBuffer();
	renderer.ReleaseShader();
	renderer.Release();

	return 0;
}
