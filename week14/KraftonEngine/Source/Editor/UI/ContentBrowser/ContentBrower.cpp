#include "ContentBrower.h"

#include "WICTextureLoader.h"


void DefaultElement::Render(ContentBrowserContext& Context)
{
	ImGui::SetNextWindowSize(Context.ContentSize);
	ImGui::BeginChild("ElementArea", ImVec2(0, 0), true);
	ImGui::Image((ImTextureID)Icon, Context.ContentSize);
	ImGui::Text(FPaths::ToUtf8(ContentItem.Name).c_str());
	ImGui::EndChild();

}

void FEditorContextBrwoserWidget::Initialize(UEditorEngine* InEditor, ID3D11Device* InDevice)
{
	FEditorWidget::Initialize(InEditor);
	if (!InDevice) return;

	const std::wstring IconDir = FPaths::Combine(FPaths::RootDir(), L"Asset/Editor/Icons/");

	DirectX::CreateWICTextureFromFile(
		InDevice, (IconDir + L"StartMerge_42x.png").c_str(),
		nullptr, &DefaultIcon);

	DirectX::CreateWICTextureFromFile(
		InDevice, (IconDir + L"Folder_Base_256x.png").c_str(),
		nullptr, &FolderIcon);

	ContentBrowserContext Context;
	Context.ContentSize = ImVec2(50, 50);
	BrowserContext = Context;

	Refresh();
}

void FEditorContextBrwoserWidget::Render(float DeltaTime)
{
	if (!ImGui::Begin("ContentBrowser"))
	{
		ImGui::End();
		return;
	}

	if (ImGui::Button("Refresh"))
		Refresh();

	if (!ImGui::BeginTable("ContentBrowserLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::EndTable();
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

void FEditorContextBrwoserWidget::Refresh()
{
	RootNode = BuildDirectoryTree(FPaths::RootDir());
	RefreshContent();
}

void FEditorContextBrwoserWidget::RefreshContent()
{
	CachedBrowserElements.clear();
	TArray<FContentItem> CurrentContents = ReadDirectory(CurrentPath);
	for (const auto& Content : CurrentContents)
	{
		auto element = std::make_unique<DefaultElement>();
		element.get()->SetContent(Content);
		element.get()->SetIcon(DefaultIcon);

		CachedBrowserElements.push_back(std::move(element));
	}
}

void FEditorContextBrwoserWidget::DrawDirNode(FDirNode InNode)
{
	ImGuiTreeNodeFlags Flag = InNode.Children.empty() ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_OpenOnArrow;

	bool bIsOpen = ImGui::TreeNodeEx(FPaths::ToUtf8(InNode.Self.Name).c_str(), Flag);
	if (ImGui::IsItemClicked())
	{
		CurrentPath = InNode.Self.Path;
		RefreshContent();
	}

	if (!bIsOpen)
	{
		return;
	}

	int32 ChildrenCount = InNode.Children.size();
	for (int i = 0; i < ChildrenCount; i++)
	{
		DrawDirNode(InNode.Children[i]);
	}

	ImGui::TreePop();
}

void FEditorContextBrwoserWidget::DrawContents()
{
	int elementCount = CachedBrowserElements.size();
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

TArray<FContentItem> FEditorContextBrwoserWidget::ReadDirectory(std::wstring Path)
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

	return Items;
}

FDirNode FEditorContextBrwoserWidget::BuildDirectoryTree(const std::filesystem::path& DirPath)
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
