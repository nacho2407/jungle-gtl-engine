#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "Render/Renderer/Renderer.h"
#include "Engine/Core/InputSystem.h"

void FEditorMainPanel::Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UEditorEngine* InEditorEngine)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& IO = ImGui::GetIO();
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	Window = InWindow;

	// 한글 지원 폰트 로드 (시스템 맑은 고딕)
	IO.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 16.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());

	ImGui_ImplWin32_Init((void*)InWindow->GetHWND());
	ImGui_ImplDX11_Init(InRenderer.GetFD3DDevice().GetDevice(), InRenderer.GetFD3DDevice().GetDeviceContext());

	ConsoleWidget.Initialize(InEditorEngine);
	ControlWidget.Initialize(InEditorEngine);
	PropertyWidget.Initialize(InEditorEngine);
	SceneWidget.Initialize(InEditorEngine);
	ViewportOverlayWidget.Initialize(InEditorEngine);
	StatWidget.Initialize(InEditorEngine);

	for (int32 i = 0; i < MaxViewports; ++i)
	{
		ViewportWidgets[i].Initialize(InEditorEngine);
		ViewportWidgets[i].SetIndex(i);
	}
}

void FEditorMainPanel::Release()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void FEditorMainPanel::Render(float DeltaTime)
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

	for (int32 i = 0; i < ActiveViewportCount; ++i)
	{
		ViewportWidgets[i].Render(DeltaTime);
	}
	ConsoleWidget.Render(DeltaTime);
	ControlWidget.Render(DeltaTime);
	PropertyWidget.Render(DeltaTime);
	SceneWidget.Render(DeltaTime);
	ViewportOverlayWidget.Render(DeltaTime);
	StatWidget.Render(DeltaTime);

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FEditorMainPanel::Update()
{
	ImGuiIO& IO = ImGui::GetIO();

	InputSystem::Get().GetGuiInputState().bUsingMouse = IO.WantCaptureMouse;
	InputSystem::Get().GetGuiInputState().bUsingKeyboard = IO.WantCaptureKeyboard;

	// IME는 ImGui가 텍스트 입력을 원할 때만 활성화.
	// 그 외에는 OS 수준에서 IME 컨텍스트를 NULL로 연결해 한글 조합이
	// 뷰포트에 남는 현상을 원천 차단한다.
	if (Window)
	{
		HWND hWnd = Window->GetHWND();
		if (IO.WantTextInput)
		{
			// InputText 포커스 중 — 기본 IME 컨텍스트 복원
			ImmAssociateContextEx(hWnd, NULL, IACE_DEFAULT);
		}
		else
		{
			// InputText 포커스 없음 — IME 컨텍스트 해제 (조합 불가)
			ImmAssociateContext(hWnd, NULL);
		}
	}
}
