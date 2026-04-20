#include "ContentBrower.h"


void DefaultElement::Render(ContentBrowserContext& Context)
{
	ImGui::Button("TestButton", Context.ContentSize);
}

void FEditorContextBrwoserWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);

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
		CachedBrowserElements.push_back(std::make_unique<DefaultElement>());
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
