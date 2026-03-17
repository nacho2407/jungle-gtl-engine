#include "Editor.h"
#include "EditorConsole.h"
#include "EditorControlPanel.h"
#include "EditorPropertyPanel.h"
#include "EditorStatPanel.h"
#include "EditorToolbar.h"
#include "Gizmo.h"
#include "GizmoController.h"
#include "Component/SceneComponent.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

Editor::Editor()
{
	ControlPanel = new EditorControlPanel(this);
	Console = new EditorConsole();
	PropertyPanel = new EditorPropertyPanel();
	StatPanel = new EditorStatPanel();
}

Editor::~Editor()
{
	delete ControlPanel;
	delete Console;
	delete PropertyPanel;
	delete StatPanel;
}

bool Editor::Initialize(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* deviceContext)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(device, deviceContext);

	return true;
}

void Editor::OnBeforeRender()
{
	if (SelectedComponent == nullptr) return;

	Gizmo::GetInstance().UpdateRenderObject();
	Gizmo::GetInstance().OnBeforeRender(Camera);
}

void Editor::DrawUI()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	DrawControlPanel();
	DrawConsole();
	DrawPropertyPanel();
	DrawStatPanel();
	DrawToolbar();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void Editor::Finalize()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void Editor::SelectComponent(USceneComponent* component)
{
	if (SelectedComponent)
	{
		SelectedComponent->bIsSelected = false;
		SelectedComponent = nullptr;
		Gizmo::GetInstance().GetController()->AttachTo(nullptr);
	}
	if (component)
	{
		component->bIsSelected = true;
		SelectedComponent = component;
		Gizmo::GetInstance().GetController()->AttachTo(component);
	}
}

void Editor::DrawControlPanel()
{
	ControlPanel->Draw(Camera);
}

void Editor::DrawConsole()
{
	bool flag = true;
	Console->Draw("Example: Console", &flag);
}

void Editor::DrawPropertyPanel()
{
	PropertyPanel->Draw(SelectedComponent);
}

void Editor::DrawStatPanel()
{
	StatPanel->Draw();
}

void Editor::DrawToolbar()
{
	Toolbar->Draw();
}
