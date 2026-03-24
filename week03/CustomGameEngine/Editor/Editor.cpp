#include "Editor.h"
#include "ThirdParty/json.h"
#include "Component/BillboardTextComponent.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "ImGui/imgui_internal.h"
#include "World.h"
#include <filesystem>
#include <fstream>
#include <streambuf>

Editor::Editor()
{
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
	delete Toolbar;
	delete SceneMgrPanel;
}

bool Editor::Initialize(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* deviceContext)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	const std::filesystem::path iniPath = std::filesystem::absolute("imgui.ini");
	ImGuiIniPath = iniPath.string();
	io.IniFilename = ImGuiIniPath.c_str();

	ImFont* DefaultFont = io.Fonts->AddFontFromFileTTF("Assets/Fonts/D2CodingSubset.ttf", 18.0f);
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(device, deviceContext);

	return true;
}

void Editor::DrawUI()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	DrawDockSpace();

	DrawControlPanel();
	DrawConsole();
	DrawPropertyPanel();
	DrawStatPanel();
	DrawToolbar();
	DrawSceneManagerPanel();

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

void Editor::DrawDockSpace()
{
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_MenuBar;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("MainDockSpace", nullptr, windowFlags);

	ImGui::PopStyleVar(3);

	ImGuiID dockspaceID = ImGui::GetID("EditorDockSpace");
	ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
	ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), dockspaceFlags);

	if (!bDockLayoutInitialized)
	{
		//SetupDefaultDockLayout(dockspaceID); //ini 파일을 무시하고 IMGUI 기본 옵션으로 레이아웃 설정
		bDockLayoutInitialized = true;
	}

	ImGui::End();
}

void Editor::SetupDefaultDockLayout(ImGuiID dockspaceID)
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::DockBuilderRemoveNode(dockspaceID);
	ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
	ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->WorkSize);

	ImGuiID dockMain = dockspaceID;
	ImGuiID dockLeft = 0;
	ImGuiID dockRight = 0;
	ImGuiID dockBottom = 0;
	ImGuiID dockToolbar = 0;

	ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.28f, &dockLeft, &dockMain);
	ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.24f, &dockRight, &dockMain);
	ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.26f, &dockBottom, &dockMain);
	ImGui::DockBuilderSplitNode(dockLeft, ImGuiDir_Down, 0.18f, &dockToolbar, &dockLeft);

	ImGui::DockBuilderDockWindow("Jungle Control Panel", dockLeft);
	ImGui::DockBuilderDockWindow("Toolbar", dockToolbar);
	ImGui::DockBuilderDockWindow("Example: Console", dockBottom);
	ImGui::DockBuilderDockWindow("Jungle Property Panel", dockRight);
	ImGui::DockBuilderDockWindow("Scene Manager", dockRight);
	ImGui::DockBuilderDockWindow("Jungle Stats Panel", dockRight);

	ImGui::DockBuilderFinish(dockspaceID);
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

void Editor::ClearSelection()
{
	for (USceneComponent* component : SelectedComponents)
	{
		if (component == nullptr)
		{
			continue;
		}

		// 선택 해제 시 라벨 같이 제거함
		RemoveBillboardTextForSelection(component);
		component->bIsSelected = false;
		if (UPrimitiveComponent* primitive = Cast<UPrimitiveComponent>(component))
		{
			primitive->MarkRenderStateDirty();
		}
	}
	SelectedComponents.clear();
	if (SelectedComponents.empty())
	{
		SelectedComponents.shrink_to_fit();
	}
	UpdateGizmoSelection();
}

void Editor::SelectSingleComponent(USceneComponent* component)
{
	ClearSelection();

	if (component)
	{
		// 선택 시 라벨 보장함
		EnsureBillboardTextForSelection(component);
		component->bIsSelected = true;
		if (UPrimitiveComponent* primitive = Cast<UPrimitiveComponent>(component))
		{
			primitive->MarkRenderStateDirty();
		}
		SelectedComponents.push_back(component);
		UpdateGizmoSelection();
	}
}

void Editor::SelectMultiComponent(USceneComponent* component)
{
	if (component == nullptr)
	{
		return;
	}

	for (auto it = SelectedComponents.begin(); it != SelectedComponents.end(); ++it)
	{
		if (*it != component)
		{
			continue;
		}

		// 토글 해제 시 라벨 제거함
		RemoveBillboardTextForSelection(component);
		component->bIsSelected = false;
		if (UPrimitiveComponent* primitive = Cast<UPrimitiveComponent>(component))
		{
			primitive->MarkRenderStateDirty();
		}
		SelectedComponents.erase(it);
		UpdateGizmoSelection();
		return;
	}

	// 새로 선택된 대상 라벨 보장함
	EnsureBillboardTextForSelection(component);
	component->bIsSelected = true;
	if (UPrimitiveComponent* primitive = Cast<UPrimitiveComponent>(component))
	{
		primitive->MarkRenderStateDirty();
	}
	SelectedComponents.push_back(component);
	UpdateGizmoSelection();
}

void Editor::SelectAllComponents(const TArray<USceneComponent*>& components)
{
	ClearSelection();

	for (USceneComponent* component : components)
	{
		if (component == nullptr)
		{
			continue;
		}

		if (Cast<UBillboardTextComponent>(component) != nullptr)
		{
			continue;
		}

		component->bIsSelected = true;
		if (UPrimitiveComponent* primitive = Cast<UPrimitiveComponent>(component))
		{
			primitive->MarkRenderStateDirty();
		}
		SelectedComponents.push_back(component);
	}

	UpdateGizmoSelection();
}

void Editor::RemoveSelectedComponents()
{
	TArray<USceneComponent*> selectedComponents = GetSelectedComponents();
	ClearSelection();

	SceneManager* sceneManager = GetWorld().GetSceneManager();
	if (sceneManager == nullptr)
	{
		return;
	}

	for (USceneComponent* comp : selectedComponents)
	{
		if (comp == nullptr || comp->Scene == nullptr)
		{
			continue;
		}
		sceneManager->RemoveSceneComponent(comp, comp->Scene);
	}
}

//마지막으로 선택된 씬 컴포넌트를 반환합니다. 그걸로 기즈모를 조작하기 위해서입니다.
USceneComponent* Editor::GetLastSelectedComponent() const
{
	if (SelectedComponents.empty())
	{
		return nullptr;
	}
	return SelectedComponents.back();
}

/**
 * SelectedComponents 배열을 돌며 현재 컴포넌트가 속해 있는지 확인합니다..
 * 
 * \param component
 * \return 
 */
bool Editor::IsComponentSelected(USceneComponent* component) const
{
	for (USceneComponent* selectedComponent : SelectedComponents)
	{
		if (selectedComponent == component)
		{
			return true;
		}
	}

	return false;
}

UBillboardTextComponent* Editor::FindBillboardTextComponent(USceneComponent* component) const
{
	if (component == nullptr || component->Scene == nullptr)
	{
		return nullptr;
	}

	for (UBillboardTextComponent* BillboardText : component->Scene->BillboardTextComponents)
	{
		if (BillboardText != nullptr && BillboardText->GetAttachedComponent() == component)
		{
			return BillboardText;
		}
	}

	return nullptr;
}

/**
 * @brief 선택된 scene component용 billboard text component를 생성하거나 재사용합니다.
 *
 * @param component 선택된 대상 scene component
 */
void Editor::EnsureBillboardTextForSelection(USceneComponent* component)
{
	if (component == nullptr || component->Scene == nullptr)
	{
		return;
	}

	if (Cast<UBillboardTextComponent>(component) != nullptr)
	{
		return;
	}

	if (FindBillboardTextComponent(component) != nullptr)
	{
		return;
	}

	SceneManager* SceneMgr = GetWorld().GetSceneManager();
	if (SceneMgr == nullptr)
	{
		return;
	}

	// 선택된 대상에만 라벨 생성함
	UBillboardTextComponent* BillboardText = SceneMgr->AddSceneComponent<UBillboardTextComponent>(component->Scene);
	if (BillboardText != nullptr)
	{
		BillboardText->AttachTo(component);
	}
}

/**
 * @brief 선택 해제된 scene component용 billboard text component를 제거합니다.
 *
 * @param component 선택 해제된 대상 scene component
 */
void Editor::RemoveBillboardTextForSelection(USceneComponent* component)
{
	if (component == nullptr || component->Scene == nullptr)
	{
		return;
	}

	SceneManager* SceneMgr = GetWorld().GetSceneManager();
	if (SceneMgr == nullptr)
	{
		return;
	}

	UBillboardTextComponent* BillboardText = FindBillboardTextComponent(component);
	if (BillboardText == nullptr)
	{
		return;
	}

	// SceneManager 경유해서 소유권 정리함
	SceneMgr->RemoveSceneComponent(BillboardText, component->Scene);
}

void Editor::UpdateGizmoSelection()
{
	UGizmoComponent* controller = Gizmo::GetInstance().GetController();
	if (controller == nullptr)
	{
		return;
	}

	controller->SetControlledComponents(SelectedComponents);
	controller->AttachTo(GetLastSelectedComponent());
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

void Editor::DrawConsole()
{
	bool flag = true;
	Console->Draw("Example: Console", &flag);
}

void Editor::DrawControlPanel()
{
	ControlPanel->Draw(Camera, Grid);
}

void Editor::DrawPropertyPanel()
{
	PropertyPanel->Draw(GetLastSelectedComponent());
}

void Editor::DrawStatPanel()
{
	StatPanel->Draw();
}

void Editor::DrawToolbar()
{
	Toolbar->Draw(Grid);
}

void Editor::DrawSceneManagerPanel()
{
	SceneMgrPanel->Draw(this);
}
