#include "ContentBrowser.h"

#include "ContentBrowserElement.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Subsystem/AssetFactory.h"
#include "Editor/UI/EditorTextureManager.h"
#include "FloatCurve/FloatCurveAsset.h"
#include "FloatCurve/FloatCurveManager.h"
#include "EditorEngine.h"

#include <algorithm>

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

	bool IsSubPath(const std::filesystem::path& Parent, const std::filesystem::path& Child)
	{
		std::filesystem::path P = std::filesystem::weakly_canonical(Parent);
		std::filesystem::path C = std::filesystem::weakly_canonical(Child);

		auto PIt = P.begin();
		auto CIt = C.begin();

		for (; PIt != P.end() && CIt != C.end(); ++PIt, ++CIt)
		{
			if (*PIt != *CIt)
			{
				return false;
			}
		}

		return PIt == P.end();
	}
}

void FEditorContentBrowserWidget::Initialize(UEditorEngine* InEditor, ID3D11Device* InDevice)
{
	FEditorWidget::Initialize(InEditor);
	if (!InDevice) return;

	IconFileMap[".Scene"] = L"World_64x.png";
	IconFileMap[".obj"] = L"icon_MatEd_Mesh_40x.png";
	IconFileMap[".mat"] = L"Sphere_64x.png";
	IconFileMap[".curve"] = L"StartMerge_42x.png";
	IconFileMap[".shake"] = L"StartMerge_42x.png";
	IconFileMap[".fbx"] = L"icon_MatEd_Mesh_40x.png";

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

	(void)DeltaTime;

	if (BrowserContext.bPendingContentRefresh)
	{
		RefreshContent();
		BrowserContext.bPendingContentRefresh = false;
	}

	ImGui::SameLine();
	std::wstring PathText = BrowserContext.CurrentPath;
	if (BrowserContext.SelectedElement)
		PathText += L"/" + BrowserContext.SelectedElement->GetFileName();

	ImGui::SameLine();
	int Size = static_cast<int>(BrowserContext.ContentSize.x);
	BrowserContext.ContentSize = ImVec2(static_cast<float>(Size), static_cast<float>(Size));

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

	BrowserContext.bPendingContentRefresh = false;
}

void FEditorContentBrowserWidget::SetIconSize(float Size)
{
	const float ClampedSize = (std::max)(20.0f, (std::min)(Size, 100.0f));
	BrowserContext.ContentSize = ImVec2(ClampedSize, ClampedSize);
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
	FEditorTextureManager::Get().ClearThumbnails();
	CachedBrowserElements.clear();
	TArray<FContentItem> CurrentContents = ReadDirectory(BrowserContext.CurrentPath);
	for (const auto& Content : CurrentContents)
	{
		std::shared_ptr<ContentBrowserElement> Element;
		FString Extension = FPaths::ToUtf8(Content.Path.extension());
		for (char& Character : Extension)
		{
			Character = static_cast<char>(std::tolower(static_cast<unsigned char>(Character)));
		}
		ID3D11ShaderResourceView* Icon = nullptr;

		if (Content.bIsDirectory)
		{
			Element = std::make_shared<DirectoryElement>();
			Icon = FEditorTextureManager::Get().GetOrLoadIcon(FPaths::ToUtf8(FPaths::Combine(FPaths::AssetDir(), L"Editor/Icons/", L"Folder_Base_256x.png")));
		}
		else if (Extension == ".scene")
		{
			Element = std::make_shared<SceneElement>();
		}
		else if (Extension == ".obj")
		{
			Element = std::make_shared<ObjectElement>();
		}
		else if (Extension == ".mat")
		{
			Element = std::make_shared<MaterialElement>();
		}
		else if (Extension == ".curve")
		{
			Element = std::make_shared<FloatCurveElement>();
		}
		else if (Extension == ".shake")
		{
			Element = std::make_shared<CameraShakeElement>();
		}
		else if (Extension == ".fbx")
		{
			Element = std::make_shared<MeshElement>();
		}
		else if (Extension == ".png")
		{
			Element = std::make_shared<PNGElement>();
			Icon = FEditorTextureManager::Get().GetOrLoadThumbnail(FPaths::ToUtf8(Content.Path.lexically_relative(FPaths::RootDir()).generic_wstring()));
		}
		else
		{
			Element = std::make_shared<ContentBrowserElement>();
		}

		if (!Icon)
		{
			std::wstring IconFileName = L"StartMerge_42x.png";
			if (auto It = IconFileMap.find(Extension); It != IconFileMap.end())
			{
				IconFileName = It->second;
			}

			Icon = FEditorTextureManager::Get().GetOrLoadIcon(FPaths::ToUtf8(FPaths::Combine(FPaths::AssetDir(), L"Editor/Icons/", IconFileName)));
		}

		Element->SetIcon(Icon);

		Element->SetContent(Content);
		CachedBrowserElements.push_back(std::move(Element));
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
		if (BrowserContext.CurrentPath != InNode.Self.Path)
		{
			BrowserContext.CurrentPath = InNode.Self.Path;
			RefreshContent();
		}
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
	int ElementCount = static_cast<int>(CachedBrowserElements.size());

	const float ContentWidth = ImGui::GetContentRegionAvail().x;
	const float ItemWidth = BrowserContext.ContentSize.x;
	const float ItemHeight = BrowserContext.ContentSize.y;

	int ColumnCount = static_cast<int>(ContentWidth / ItemWidth);
	if (ColumnCount < 1)
	{
		ColumnCount = 1;
	}

	float GapSize = 0.0f;
	if (ColumnCount > 1)
	{
		GapSize = (ContentWidth - ItemWidth * ColumnCount) / (ColumnCount);
	}

	ImVec2 StartPos = ImGui::GetCursorPos();

	for (int i = 0; i < ElementCount; ++i)
	{
		int Column = i % ColumnCount;
		int Row = i / ColumnCount;

		float X = StartPos.x + Column * (ItemWidth + GapSize);
		float Y = StartPos.y + Row * (ItemHeight + GapSize * 2.f);

		ImGui::SetCursorPos(ImVec2(X, Y));
		CachedBrowserElements[i]->Render(BrowserContext);
	}

	int RowCount = (ElementCount + ColumnCount - 1) / ColumnCount;
	ImGui::SetCursorPos(ImVec2(StartPos.x, StartPos.y + RowCount * ItemHeight));

	if (ImGui::BeginPopupContextWindow("##ContentBrowserBackgroundContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::BeginMenu("Create"))
		{
			if (ImGui::MenuItem("Float Curve"))
			{
				FString CreatedPath;
				if (FAssetFactory::CreateFloatCurve(FPaths::ToUtf8(BrowserContext.CurrentPath), "NewFloatCurve", CreatedPath))
				{
					Refresh();
					if (BrowserContext.EditorEngine)
					{
						if (UFloatCurveAsset* CurveAsset = FFloatCurveManager::Get().Load(CreatedPath))
						{
							BrowserContext.EditorEngine->OpenAssetEditorForObject(CurveAsset);
						}
					}
				}
			}
			ImGui::EndMenu();
		}

		ImGui::Separator();
		if (ImGui::MenuItem("Refresh"))
		{
			Refresh();
		}

		ImGui::EndPopup();
	}
}

TArray<FContentItem> FEditorContentBrowserWidget::ReadDirectory(std::wstring Path)
{
	TArray<FContentItem> Items;

	if (!std::filesystem::exists(Path) || !std::filesystem::is_directory(Path))
		return Items;

	for (const auto& Entry : std::filesystem::directory_iterator(Path))
	{
		std::wstring Name = Entry.path().filename().wstring();
		if (Entry.is_directory())
		{
			if (Name == L"Bin" || Name == L"Build" || Name == L".git" || Name == L".vs")
				continue;
		}

		FContentItem Item;
		Item.Path = Entry.path();
		Item.Name = Name;
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

		std::wstring DirName = Entry.path().filename().wstring();
		if (DirName == L"Bin" || DirName == L"Build" || DirName == L".git" || DirName == L".vs")
			continue;

		Node.Children.push_back(BuildDirectoryTree(Entry.path()));
	}

	if (Node.Self.Name.empty())
		Node.Self.Name = FPaths::ToWide("Project");

	return Node;
}
