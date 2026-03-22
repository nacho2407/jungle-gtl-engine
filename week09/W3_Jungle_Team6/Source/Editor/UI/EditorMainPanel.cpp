#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "Render/Renderer/Renderer.h"
#include "Engine/Core/InputSystem.h"


#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

void FEditorMainPanel::Create(HWND InHWindow, FRenderer& InRenderer, FEditorEngine* InEditorEngine)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& IO = ImGui::GetIO();
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui_ImplWin32_Init((void*)InHWindow);
	ImGui_ImplDX11_Init(InRenderer.GetFD3DDevice().GetDevice(), InRenderer.GetFD3DDevice().GetDeviceContext());

	ConsoleWidget.Initialize(InEditorEngine);
	ControlWidget.Initialize(InEditorEngine);
	PropertyWidget.Initialize(InEditorEngine);
	SceneWidget.Initialize(InEditorEngine);
	ViewportOverlayWidget.Initialize(InEditorEngine);
}

void FEditorMainPanel::Release()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void FEditorMainPanel::Render(float DeltaTime, FViewOutput& ViewOutput)
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

	ConsoleWidget.Render(DeltaTime, ViewOutput);
	ControlWidget.Render(DeltaTime, ViewOutput);
	PropertyWidget.Render(DeltaTime, ViewOutput);
	SceneWidget.Render(DeltaTime, ViewOutput);
	ViewportOverlayWidget.Render(DeltaTime, ViewOutput);

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FEditorMainPanel::Update()
{
	ImGuiIO& IO = ImGui::GetIO();

	InputSystem::GuiInputState.bUsingMouse = IO.WantCaptureMouse;
	InputSystem::GuiInputState.bUsingKeyboard = IO.WantCaptureKeyboard;
}
