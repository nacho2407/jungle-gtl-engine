#include "Editor.h"

Editor::Editor()
{
	UILayout = ImGuiLayout();

	ControlPanel = new EditorControlPanel(this, Renderer);
	Console = new EditorConsole();
	PropertyPanel = new EditorPropertyPanel(this);
	StatPanel = new EditorStatPanel();
	Toolbar = new EditorToolbar();
	SceneMgrPanel = new EditorSceneManagerPanel();
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
	ImFont* DefaultFont = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/malgun.ttf", 18.0f);
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(device, deviceContext);

	return true;
}

void Editor::DrawUI()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	DrawControlPanel(UILayout);
	DrawConsole(UILayout);
	DrawPropertyPanel(UILayout);
	DrawStatPanel(UILayout);
	DrawToolbar(UILayout);
	DrawSceneManagerPanel(UILayout);

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void Editor::Finalize()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void Editor::SetRenderer(URenderer* renderer)
{
	Renderer = renderer;
	if (ControlPanel)
	{
		ControlPanel->Renderer = renderer;
	}
}

void Editor::SetGrid(UGridComp* grid)
{
	Grid = grid;
	ResourceManager::GetInstance()->UpdateGridMesh(Grid->HalfExtent, Grid->CellSize, 0.f);
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

void Editor::DrawConsole(ImGuiLayout& layout)
{
	bool flag = true;
	Console->Draw("Example: Console", &flag, layout.Console);
}

void Editor::DrawControlPanel(ImGuiLayout& layout)
{
	ControlPanel->Draw(Camera, layout.ControlPanel, Grid);
}

void Editor::DrawPropertyPanel(ImGuiLayout& layout)
{
	PropertyPanel->Draw(SelectedComponent, layout.PropertyPanel);
}

void Editor::DrawStatPanel(ImGuiLayout& layout)
{
	StatPanel->Draw(layout.StatPanel);
}

void Editor::DrawToolbar(ImGuiLayout& layout)
{
	Toolbar->Draw(layout.Toolbar, Grid);
}

void Editor::DrawSceneManagerPanel(ImGuiLayout& layout)
{
	SceneMgrPanel->Draw(this, layout.SceneMgrPanel);
}
