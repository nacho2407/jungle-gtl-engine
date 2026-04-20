#include "ContentBrowser.h"

#include "ContentBrowserElement.h"
#include "WICTextureLoader.h"


void FEditorContentBrowserWidget::Initialize(UEditorEngine* InEditor, ID3D11Device* InDevice)
{
	FEditorWidget::Initialize(InEditor);
	if (!InDevice) return;

	const std::wstring IconDir = FPaths::Combine(FPaths::RootDir(), L"Asset/Editor/Icons/");

	DirectX::CreateWICTextureFromFile(
		InDevice, (IconDir + L"StartMerge_42x.png").c_str(),
		nullptr, ICons["Default"].GetAddressOf());

	DirectX::CreateWICTextureFromFile(
		InDevice, (IconDir + L"Folder_Base_256x.png").c_str(),
		nullptr, ICons["Directory"].GetAddressOf());

	DirectX::CreateWICTextureFromFile(
		InDevice, (IconDir + L"icon_landscape_40x.png").c_str(),
		nullptr, ICons[".Scene"].GetAddressOf());

	DirectX::CreateWICTextureFromFile(
		InDevice, (IconDir + L"icon_MatEd_Mesh_40x.png").c_str(),
		nullptr, ICons[".obj"].GetAddressOf());

	ContentBrowserContext Context;
	Context.ContentSize = ImVec2(50, 50);
	Context.EditorEngine = InEditor;
	BrowserContext = Context;

	Refresh();
}

void FEditorContentBrowserWidget::Render(float DeltaTime)
{
	if (!ImGui::Begin("ContentBrowser"))
	{
		ImGui::End();
		return;
	}

	if (ImGui::Button("Refresh") || BrowserContext.bIsNeedRefresh)
		Refresh();

	ImGui::SameLine();
	std::wstring PathText = BrowserContext.CurrentPath;
	if(BrowserContext.SelectedElement)
		PathText += L"/" + BrowserContext.SelectedElement->GetFileName();

	ImGui::Text(FPaths::ToUtf8(PathText).c_str());

	ImGui::SameLine();
	int size = static_cast<int>(BrowserContext.ContentSize.x);
	ImGui::SliderInt("##slider", &size, 20, 100);
	BrowserContext.ContentSize = ImVec2(static_cast<float>(size), static_cast<float>(size));

	if (!ImGui::BeginTable("ContentBrowserLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::End();
		return;
	}

	ImGui::TableSetupColumn("Directory", ImGuiTableColumnFlags_WidthFixed, 250.0f);
	ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch);

	ImGui::TableNextColumn();
	{
		ImGui::BeginChild("DirectoryTree", ImVec2(0, 0), true);
		DrawDirNode(RootNode);
		ImGui::EndChild();
	}

	ImGui::TableNextColumn();
	{
		ImGui::BeginChild("ContentArea", ImVec2(0, 0), true);
		DrawContents();
		ImGui::EndChild();
	}

	ImGui::EndTable();
	ImGui::End();
}

void FEditorContentBrowserWidget::Refresh()
{
	RootNode = BuildDirectoryTree(FPaths::RootDir());
	RefreshContent();

	BrowserContext.bIsNeedRefresh = false;
}

void FEditorContentBrowserWidget::RefreshContent()
{
	CachedBrowserElements.clear();
	TArray<FContentItem> CurrentContents = ReadDirectory(BrowserContext.CurrentPath);
	for (const auto& Content : CurrentContents)
	{
		std::unique_ptr<ContentBrowserElement> element;
		FString Extension = FPaths::ToUtf8(Content.Path.extension());

		if (Content.bIsDirectory)
		{
			element = std::make_unique<DirectoryElement>();
			element.get()->SetIcon(ICons["Directory"].Get());

		}
		else if (Content.Path.extension() == ".Scene")
		{
			element = std::make_unique<SceneElement>();
			element.get()->SetIcon(ICons[Extension].Get());
		}
		else if (Content.Path.extension() == ".obj")
		{
			element = std::make_unique<ObjectElement>();
			element.get()->SetIcon(ICons[Extension].Get());
		}
		else
		{
			element = std::make_unique<ContentBrowserElement>();
			element.get()->SetIcon(ICons["Default"].Get());
		}
		
		element.get()->SetContent(Content);
		CachedBrowserElements.push_back(std::move(element));
	}

	BrowserContext.SelectedElement = nullptr;
}

void FEditorContentBrowserWidget::DrawDirNode(FDirNode InNode)
{
	ImGuiTreeNodeFlags Flag = InNode.Children.empty() ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_OpenOnArrow;

	bool bIsOpen = ImGui::TreeNodeEx(FPaths::ToUtf8(InNode.Self.Name).c_str(), Flag);
	if (ImGui::IsItemClicked())
	{
		BrowserContext.CurrentPath = InNode.Self.Path;
		RefreshContent();
	}

	if (!bIsOpen)
	{
		return;
	}

	int32 ChildrenCount = static_cast<int32>(InNode.Children.size());
	for (int i = 0; i < ChildrenCount; i++)
	{
		DrawDirNode(InNode.Children[i]);
	}

	ImGui::TreePop();
}

void FEditorContentBrowserWidget::DrawContents()
{
	int elementCount = static_cast<int>(CachedBrowserElements.size());
	float window_visible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

	for (int i = 0; i < elementCount; i++)
	{
		CachedBrowserElements[i]->Render(BrowserContext);

		float last_button_x2 = ImGui::GetItemRectMax().x;
		float next_button_x2 = last_button_x2 + ImGui::GetStyle().ItemSpacing.x + 32;

		if (i + 1 < BrowserContext.ContentSize.x && next_button_x2 < window_visible_x2)
			ImGui::SameLine();
	}
}

TArray<FContentItem> FEditorContentBrowserWidget::ReadDirectory(std::wstring Path)
{
	TArray<FContentItem> Items;

	if (!std::filesystem::exists(Path))
		return Items;

	if (!std::filesystem::is_directory(Path))
		return Items;

	for (const auto& Entry : std::filesystem::directory_iterator(Path))
	{
		FContentItem Item;
		Item.Path = Entry.path();
		Item.Name = Entry.path().filename().wstring();
		Item.bIsDirectory = Entry.is_directory();

		Items.push_back(Item);
	}

	std::sort(Items.begin(), Items.end(),
		[](const FContentItem& A, const FContentItem& B)
		{
			if (A.bIsDirectory != B.bIsDirectory)
				return A.bIsDirectory > B.bIsDirectory;

			return A.Name < B.Name;
		});

	return Items;
}

FEditorContentBrowserWidget::FDirNode FEditorContentBrowserWidget::BuildDirectoryTree(const std::filesystem::path& DirPath)
{
	FDirNode Node;
	Node.Self.Path = DirPath;
	Node.Self.Name = DirPath.filename().wstring();
	Node.Self.bIsDirectory = true;

	for (const auto& Entry : std::filesystem::directory_iterator(DirPath))
	{
		if (!Entry.is_directory())
			continue;

		Node.Children.push_back(BuildDirectoryTree(Entry.path()));
	}

	if(Node.Self.Name.empty())
		Node.Self.Name = FPaths::ToWide("<Unnamed>");

	return Node;
}
