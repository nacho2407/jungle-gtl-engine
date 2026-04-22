#include "ContentBrowser.h"

#include "ContentBrowserElement.h"
#include "Editor/Settings/EditorSettings.h"
#include "WICTextureLoader.h"
#include "Resource/ResourceManager.h"

namespace
{
	bool IsParentDirectoryReference(const std::filesystem::path& Path)
	{
		for (const std::filesystem::path& Part : Path)
		{
			if (Part == L"..")
			{
				return true;
			}
		}

		return false;
	}

	FString MakeContentBrowserSettingsPath(const std::wstring& CurrentPath)
	{
		const std::filesystem::path RootPath = std::filesystem::path(FPaths::RootDir()).lexically_normal();
		const std::filesystem::path Path = std::filesystem::path(CurrentPath).lexically_normal();
		const std::filesystem::path RelativePath = Path.lexically_relative(RootPath);

		if (!RelativePath.empty() && !IsParentDirectoryReference(RelativePath))
		{
			return FPaths::ToUtf8(RelativePath.generic_wstring());
		}

		return FPaths::ToUtf8(Path.wstring());
	}

	std::wstring ResolveContentBrowserSettingsPath(const FString& SavedPath)
	{
		if (SavedPath.empty())
		{
			return FPaths::RootDir();
		}

		std::filesystem::path Path(FPaths::ToWide(SavedPath));
		if (!Path.is_absolute())
		{
			Path = std::filesystem::path(FPaths::RootDir()) / Path;
		}

		Path = Path.lexically_normal();
		if (std::filesystem::exists(Path) && std::filesystem::is_directory(Path))
		{
			return Path.wstring();
		}

		return FPaths::RootDir();
	}

	bool IsSubPath(const std::filesystem::path& parent, const std::filesystem::path& child)
	{
		std::filesystem::path p = std::filesystem::weakly_canonical(parent);
		std::filesystem::path c = std::filesystem::weakly_canonical(child);

		auto pIt = p.begin();
		auto cIt = c.begin();

		for (; pIt != p.end() && cIt != c.end(); ++pIt, ++cIt)
		{
			if (*pIt != *cIt)
				return false;
		}

		return pIt == p.end(); // parent 끝까지 다 맞았으면 포함됨
	}
}

void FEditorContentBrowserWidget::Initialize(UEditorEngine* InEditor, ID3D11Device* InDevice)
{
	FEditorWidget::Initialize(InEditor);
	if (!InDevice) return;

	const std::wstring IconDir = L"Asset/Editor/Icons/";

	ICons["Default"] = FResourceManager::Get().FindLoadedTexture(FPaths::ToUtf8(IconDir + L"StartMerge_42x.png"));
	ICons["Directory"] = FResourceManager::Get().FindLoadedTexture(FPaths::ToUtf8(IconDir + L"Folder_Base_256x.png"));
	ICons[".Scene"] = FResourceManager::Get().FindLoadedTexture(FPaths::ToUtf8(IconDir + L"World_64x.png"));
	ICons[".obj"] = FResourceManager::Get().FindLoadedTexture(FPaths::ToUtf8(IconDir + L"icon_MatEd_Mesh_40x.png"));
	ICons[".mat"] = FResourceManager::Get().FindLoadedTexture(FPaths::ToUtf8(IconDir + L"Sphere_64x.png"));

	ContentBrowserContext Context;
	Context.ContentSize = ImVec2(50, 50);
	Context.EditorEngine = InEditor;
	BrowserContext = Context;
	LoadFromSettings();

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
		BrowserContext.PendingRevealPath.clear();
		ImGui::EndChild();
	}

	ImGui::TableNextColumn();
	{
		ImGui::BeginChild("ContentArea", ImVec2(0, 0), true);
		DrawContents();
		ImGui::EndChild();
	}

	if (BrowserContext.SelectedElement)
		BrowserContext.SelectedElement->RenderDetail();

	ImGui::EndTable();
	ImGui::End();
}

void FEditorContentBrowserWidget::Refresh()
{
	RootNode = BuildDirectoryTree(FPaths::RootDir());
	RefreshContent();

	BrowserContext.bIsNeedRefresh = false;
}

void FEditorContentBrowserWidget::LoadFromSettings()
{
	BrowserContext.CurrentPath = ResolveContentBrowserSettingsPath(FEditorSettings::Get().ContentBrowserPath);
	BrowserContext.PendingRevealPath = BrowserContext.CurrentPath;
}

void FEditorContentBrowserWidget::SaveToSettings() const
{
	FEditorSettings::Get().ContentBrowserPath = MakeContentBrowserSettingsPath(BrowserContext.CurrentPath);
}

void FEditorContentBrowserWidget::RefreshContent()
{
	CachedBrowserElements.clear();
	TArray<FContentItem> CurrentContents = ReadDirectory(BrowserContext.CurrentPath);
	for (const auto& Content : CurrentContents)
	{
		std::shared_ptr<ContentBrowserElement> element;
		FString Extension = FPaths::ToUtf8(Content.Path.extension());

		if (Content.bIsDirectory)
		{
			element = std::make_shared<DirectoryElement>();
			element.get()->SetIcon(ICons["Directory"].Get());

		}
		else if (Content.Path.extension() == ".Scene")
		{
			element = std::make_shared<SceneElement>();
			element.get()->SetIcon(ICons[Extension].Get());
		}
		else if (Content.Path.extension() == ".obj")
		{
			element = std::make_shared<ObjectElement>();
			element.get()->SetIcon(ICons[Extension].Get());
		}
		else if (Content.Path.extension() == ".mat")
		{
			element = std::make_shared<MaterialElement>();
			element.get()->SetIcon(ICons[Extension].Get());
		}
		else if (Content.Path.extension() == ".png" || Content.Path.extension() == ".PNG")
		{
			element = std::make_shared<PNGElement>();
			element.get()->SetIcon(FResourceManager::Get().FindLoadedTexture(FPaths::ToUtf8(Content.Path.lexically_relative(FPaths::RootDir()).generic_wstring())).Get());
		}
		else
		{
			element = std::make_shared<ContentBrowserElement>();
			element.get()->SetIcon(ICons["Default"].Get());
		}
		
		element.get()->SetContent(Content);
		CachedBrowserElements.push_back(std::move(element));
	}
}

void FEditorContentBrowserWidget::DrawDirNode(FDirNode InNode)
{
	ImGuiTreeNodeFlags Flag =
		InNode.Children.empty() ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_OpenOnArrow;

	if (InNode.Self.Path == BrowserContext.CurrentPath)
	{
		Flag |= ImGuiTreeNodeFlags_Selected;
	}
	if (!BrowserContext.PendingRevealPath.empty() && IsSubPath(InNode.Self.Path, BrowserContext.PendingRevealPath))
	{
		ImGui::SetNextItemOpen(true, ImGuiCond_Always);
	}

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
		Node.Self.Name = FPaths::ToWide("Project");

	return Node;
}
