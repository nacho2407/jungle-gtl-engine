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

	// 1차: malgun.ttf — 한글 + 기본 라틴 (주 폰트)
	ImFontGlyphRangesBuilder KoreanBuilder;
	KoreanBuilder.AddRanges(IO.Fonts->GetGlyphRangesKorean());
	KoreanBuilder.AddRanges(IO.Fonts->GetGlyphRangesDefault());
	KoreanBuilder.BuildRanges(&FontGlyphRanges);
	IO.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 16.0f, nullptr, FontGlyphRanges.Data);

	// 2차: msyh.ttc — 한자 전체를 malgun이 없는 글리프에만 병합 (fallback)
	ImFontConfig MergeConfig;
	MergeConfig.MergeMode = true;
	IO.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 16.0f, &MergeConfig, IO.Fonts->GetGlyphRangesChineseFull());

	ImGui_ImplWin32_Init((void*)InWindow->GetHWND());
	ImGui_ImplDX11_Init(InRenderer.GetFD3DDevice().GetDevice(), InRenderer.GetFD3DDevice().GetDeviceContext());

	ConsoleWidget.Initialize(InEditorEngine);
	ControlWidget.Initialize(InEditorEngine);
	MaterialWidget.Initialize(InEditorEngine);
	PropertyWidget.Initialize(InEditorEngine);
	SceneWidget.Initialize(InEditorEngine);
	ViewportOverlayWidget.Initialize(InEditorEngine);
	StatWidget.Initialize(InEditorEngine);
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

	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

	ConsoleWidget.Render(DeltaTime);
	ControlWidget.Render(DeltaTime);
	MaterialWidget.Render(DeltaTime);
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
