#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Settings/EditorSettings.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "Render/Pipeline/Renderer.h"
#include "Engine/Input/InputSystem.h"

void FEditorMainPanel::Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UEditorEngine* InEditorEngine)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& IO = ImGui::GetIO();
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	Window = InWindow;
	EditorEngine = InEditorEngine;

	// 한글 지원 폰트 로드 (시스템 맑은 고딕)
	IO.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 16.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());

	ImGui_ImplWin32_Init((void*)InWindow->GetHWND());
	ImGui_ImplDX11_Init(InRenderer.GetFD3DDevice().GetDevice(), InRenderer.GetFD3DDevice().GetDeviceContext());

	ConsoleWidget.Initialize(InEditorEngine);
	ControlWidget.Initialize(InEditorEngine);
	PropertyWidget.Initialize(InEditorEngine);
	SceneWidget.Initialize(InEditorEngine);
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

	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

	// --- 우상단 Windows 토글 버튼 ---
	{
		FEditorSettings& S = FEditorSettings::Get();
		const ImGuiViewport* VP = ImGui::GetMainViewport();
		const float ButtonW = 80.0f;
		const float Margin = 8.0f;
		ImGui::SetNextWindowPos(ImVec2(VP->Pos.x + VP->Size.x - ButtonW - Margin, VP->Pos.y + Margin));
		ImGui::SetNextWindowSize(ImVec2(0, 0));
		ImGui::Begin("##WidgetToggle", nullptr,
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoDocking);
		if (ImGui::Button("Windows", ImVec2(ButtonW, 0)))
		{
			bShowWidgetList = !bShowWidgetList;
		}
		ImGui::End();

		if (bShowWidgetList)
		{
			ImGui::SetNextWindowPos(ImVec2(VP->Pos.x + VP->Size.x - 150.0f - Margin, VP->Pos.y + Margin + 30.0f));
			ImGui::SetNextWindowSize(ImVec2(150.0f, 0));
			ImGui::Begin("##WidgetList", &bShowWidgetList,
				ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoDocking);
			ImGui::Checkbox("Console", &S.UI.bConsole);
			ImGui::Checkbox("Control", &S.UI.bControl);
			ImGui::Checkbox("Property", &S.UI.bProperty);
			ImGui::Checkbox("Scene", &S.UI.bScene);
			ImGui::Checkbox("Stat", &S.UI.bStat);
			ImGui::End();
		}
	}

	// 뷰포트 렌더링은 EditorEngine이 담당 (SSplitter 레이아웃 + ImGui::Image)
	if (EditorEngine)
	{
		SCOPE_STAT_CAT("EditorEngine->RenderViewportUI", "5_UI");
		EditorEngine->RenderViewportUI(DeltaTime);
	}

	const FEditorSettings& Settings = FEditorSettings::Get();

	if (Settings.UI.bConsole)
	{
		SCOPE_STAT_CAT("ConsoleWidget.Render", "5_UI");
		ConsoleWidget.Render(DeltaTime);
	}

	if (Settings.UI.bControl)
	{
		SCOPE_STAT_CAT("ControlWidget.Render", "5_UI");
		ControlWidget.Render(DeltaTime);
	}

	if (Settings.UI.bProperty)
	{
		SCOPE_STAT_CAT("PropertyWidget.Render", "5_UI");
		PropertyWidget.Render(DeltaTime);
	}

	if (Settings.UI.bScene)
	{
		SCOPE_STAT_CAT("SceneWidget.Render", "5_UI");
		SceneWidget.Render(DeltaTime);
	}

	if (Settings.UI.bStat)
	{
		SCOPE_STAT_CAT("StatWidget.Render", "5_UI");
		StatWidget.Render(DeltaTime);
	}
	// 뷰포트 렌더링은 EditorEngine이 담당 (SSplitter 레이아웃 + ImGui::Image)

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FEditorMainPanel::Update()
{
	ImGuiIO& IO = ImGui::GetIO();

	// 뷰포트 슬롯 위에서는 bUsingMouse를 해제해야 TickInteraction이 동작
	bool bWantMouse = IO.WantCaptureMouse;
	if (EditorEngine && EditorEngine->IsMouseOverViewport())
	{
		bWantMouse = false;
	}
	InputSystem::Get().GetGuiInputState().bUsingMouse = bWantMouse;
	InputSystem::Get().GetGuiInputState().bUsingKeyboard = IO.WantCaptureKeyboard;

	// IME는 ImGui가 텍스트 입력을 원할 때만 활성화.
	if (Window)
	{
		HWND hWnd = Window->GetHWND();
		if (IO.WantTextInput)
		{
			ImmAssociateContextEx(hWnd, NULL, IACE_DEFAULT);
		}
		else
		{
			ImmAssociateContext(hWnd, NULL);
		}
	}
}
