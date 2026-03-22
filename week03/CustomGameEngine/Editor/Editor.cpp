#include "Editor.h"
#include "ThirdParty/json.h"
#include "Component/CameraComponent.h"
#include <fstream>
#include <streambuf>

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
	SaveSettings();
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

void Editor::LoadSettings()
{
	std::ifstream file("editor.ini");
	if (file.is_open())
	{
		std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		json::JSON data = json::JSON::Load(content);

		if (data.hasKey("Grid"))
		{
			if (Grid)
			{
				if (data["Grid"].hasKey("HalfExtent"))
					Grid->HalfExtent = (float)data["Grid"]["HalfExtent"].ToFloat();
				if (data["Grid"].hasKey("CellSize"))
					Grid->CellSize = (float)data["Grid"]["CellSize"].ToFloat();

				ResourceManager::GetInstance()->UpdateGridMesh(Grid->HalfExtent, Grid->CellSize, 0.f);
			}
		}

		if (data.hasKey("Camera"))
		{
			if (Camera)
			{
				if (data["Camera"].hasKey("MoveSpeed"))
					Camera->MoveSpeed = (float)data["Camera"]["MoveSpeed"].ToFloat();
				if (data["Camera"].hasKey("RotateSensitivity"))
					Camera->RotateSensitivity = (float)data["Camera"]["RotateSensitivity"].ToFloat();
			}
		}
	}
}

void Editor::SaveSettings()
{
	json::JSON data = json::Object();
	if (Grid)
	{
		data["Grid"] = json::Object();
		data["Grid"]["HalfExtent"] = Grid->HalfExtent;
		data["Grid"]["CellSize"] = Grid->CellSize;
	}

	if (Camera)
	{
		data["Camera"] = json::Object();
		data["Camera"]["MoveSpeed"] = Camera->MoveSpeed;
		data["Camera"]["RotateSensitivity"] = Camera->RotateSensitivity;
	}

	std::ofstream file("editor.ini");
	if (file.is_open())
	{
		file << data.dump();
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
