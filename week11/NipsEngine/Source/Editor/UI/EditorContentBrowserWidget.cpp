#include "Editor/UI/EditorContentBrowserWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/EditorRenderPipeline.h"
#include "Editor/UI/EditorMainPanel.h"
#include "Editor/Settings/EditorSettings.h"
#include "Asset/CurveFloatAsset.h"
#include "Asset/StaticMesh.h"
#include "Core/ResourceManager.h"
#include "Runtime/Script/ScriptManager.h"
#include "Render/Resource/Material.h"
#include "Render/Renderer/Renderer.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <cctype>
#include <d3d11.h>
#include <fstream>
#include <Windows.h>
#include <shellapi.h>

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

std::filesystem::path ResolveBrowserPath(const FString& SavedPath)
{
	std::filesystem::path Path;
	if (SavedPath.empty())
	{
		Path = std::filesystem::path(FPaths::RootDir()) / L"Asset";
	}
	else
	{
		Path = FPaths::ToWide(SavedPath);
		if (!Path.is_absolute())
		{
			Path = std::filesystem::path(FPaths::RootDir()) / Path;
		}
	}

	Path = Path.lexically_normal();
	if (std::filesystem::exists(Path) && std::filesystem::is_directory(Path))
	{
		return Path;
	}
	return std::filesystem::path(FPaths::RootDir()).lexically_normal();
}

FString MakeSavedBrowserPath(const std::filesystem::path& Path)
{
	const std::filesystem::path Root = std::filesystem::path(FPaths::RootDir()).lexically_normal();
	const std::filesystem::path Normalized = Path.lexically_normal();
	const std::filesystem::path Relative = Normalized.lexically_relative(Root);
	if (!Relative.empty() && !IsParentDirectoryReference(Relative))
	{
		return FPaths::ToUtf8(Relative.generic_wstring());
	}
	return FPaths::ToUtf8(Normalized.wstring());
}

FString ToLower(FString Value)
{
	std::transform(Value.begin(), Value.end(), Value.begin(),
		[](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
	return Value;
}
}

void FEditorContentBrowserWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
	RootPath = std::filesystem::path(FPaths::RootDir()).lexically_normal();
	std::error_code CreateScriptDirEc;
	std::filesystem::create_directories(RootPath / L"Asset/Script", CreateScriptDirEc);
	BrowserRootPaths.clear();
	for (const wchar_t* RootName : { L"Asset", L"LuaScript", L"Shaders" })
	{
		std::filesystem::path Candidate = (RootPath / RootName).lexically_normal();
		std::error_code Ec;
		if (std::filesystem::exists(Candidate, Ec) && std::filesystem::is_directory(Candidate, Ec))
		{
			BrowserRootPaths.push_back(Candidate);
		}
	}
	if (BrowserRootPaths.empty())
	{
		BrowserRootPaths.push_back(RootPath);
	}
	LoadFromSettings();
	Refresh();
}

void FEditorContentBrowserWidget::OpenAssetRoot()
{
	std::filesystem::path AssetRoot = (RootPath / L"Asset").lexically_normal();
	std::error_code Ec;
	if (!std::filesystem::exists(AssetRoot, Ec) || !std::filesystem::is_directory(AssetRoot, Ec))
	{
		AssetRoot = BrowserRootPaths.empty() ? RootPath : BrowserRootPaths.front();
	}

	BackHistory.clear();
	NavigateTo(AssetRoot, false);
}

void FEditorContentBrowserWidget::Render(float DeltaTime)
{
	const float TargetAlpha = bVisible ? 1.0f : 0.0f;
	const float Step = std::max(DeltaTime, ImGui::GetIO().DeltaTime) * 10.0f;
	if (AnimAlpha < TargetAlpha)
	{
		AnimAlpha = std::min(TargetAlpha, AnimAlpha + Step);
	}
	else if (AnimAlpha > TargetAlpha)
	{
		AnimAlpha = std::max(TargetAlpha, AnimAlpha - Step);
	}
	if (AnimAlpha <= 0.001f)
	{
		bMouseOverBrowser = false;
		bHasBrowserScreenRect = false;
		return;
	}

	if (bNeedsRefresh)
	{
		Refresh();
	}
	if (bPendingMaterialPreviewCacheClear)
	{
		MaterialPreviewCache.clear();
		bPendingMaterialPreviewCacheClear = false;
	}

	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	const ImVec2 WorkPos = MainViewport ? MainViewport->WorkPos : ImVec2(0.0f, 0.0f);
	const ImVec2 WorkSize = MainViewport ? MainViewport->WorkSize : ImGui::GetIO().DisplaySize;
	constexpr float FooterHeight = 32.0f;
	constexpr float DrawerMaxHeight = 380.0f;
	const bool bDrawerMode = IsDrawerMode();

	if (bDrawerMode)
	{
		const float DrawerHeight = DrawerMaxHeight * AnimAlpha;
		if (DrawerHeight <= 1.0f)
		{
			bMouseOverBrowser = false;
			bHasBrowserScreenRect = false;
			return;
		}
		ImGui::SetNextWindowPos(
			ImVec2(WorkPos.x, WorkPos.y + WorkSize.y - FooterHeight - DrawerHeight),
			ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(WorkSize.x, DrawerHeight), ImGuiCond_Always);
	}
	else
	{
		const float Width = std::min(1040.0f, WorkSize.x - 48.0f);
		const float Height = std::min(620.0f, WorkSize.y - 96.0f);
		const ImVec2 WindowPos(
			WorkPos.x + (WorkSize.x - Width) * 0.5f,
			WorkPos.y + 58.0f + (1.0f - AnimAlpha) * 20.0f);
		ImGui::SetNextWindowPos(WindowPos, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(Width, Height), ImGuiCond_FirstUseEver);
	}
	if (MainViewport)
	{
		ImGui::SetNextWindowViewport(MainViewport->ID);
	}
	ImGui::SetNextWindowBgAlpha(0.96f * AnimAlpha);

	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, AnimAlpha);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, bDrawerMode ? 0.0f : 8.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));

	bool bOpen = bVisible;
	const ImGuiWindowFlags Flags =
		ImGuiWindowFlags_NoDocking |
		(bDrawerMode
			? (ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)
			: ImGuiWindowFlags_NoCollapse);
	if (!ImGui::Begin(bDrawerMode ? "##EditorContentBrowserDrawer" : "Content Browser", &bOpen, Flags))
	{
		BrowserScreenMin = ImGui::GetWindowPos();
		BrowserScreenMax = ImVec2(BrowserScreenMin.x + ImGui::GetWindowSize().x, BrowserScreenMin.y + ImGui::GetWindowSize().y);
		bHasBrowserScreenRect = true;
		bMouseOverBrowser = IsMouseOverBrowser();
		ImGui::End();
		ImGui::PopStyleVar(3);
		bVisible = bOpen;
		return;
	}
	bVisible = bOpen;
	BrowserScreenMin = ImGui::GetWindowPos();
	BrowserScreenMax = ImVec2(BrowserScreenMin.x + ImGui::GetWindowSize().x, BrowserScreenMin.y + ImGui::GetWindowSize().y);
	bHasBrowserScreenRect = true;
	bMouseOverBrowser =
		IsMouseOverBrowser()
		|| ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

	DrawBrowserContents();

	ImGui::End();
	ImGui::PopStyleVar(3);
}

bool FEditorContentBrowserWidget::IsMouseOverBrowser() const
{
	if (AnimAlpha <= 0.001f || !bHasBrowserScreenRect)
	{
		return false;
	}

	const ImVec2 MousePos = ImGui::GetIO().MousePos;
	return MousePos.x >= BrowserScreenMin.x
		&& MousePos.x < BrowserScreenMax.x
		&& MousePos.y >= BrowserScreenMin.y
		&& MousePos.y < BrowserScreenMax.y;
}

bool FEditorContentBrowserWidget::ConsumeReleasedDragPayload(FString& OutPayloadType, FString& OutPayloadPath)
{
	if (ActiveDragPayloadPath.empty())
	{
		return false;
	}

	if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		return false;
	}

	const bool bReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
	if (bReleased)
	{
		OutPayloadType = ActiveDragPayloadType;
		OutPayloadPath = ActiveDragPayloadPath;
	}

	ActiveDragPayloadType.clear();
	ActiveDragPayloadPath.clear();
	return bReleased;
}

void FEditorContentBrowserWidget::DrawBrowserContents()
{
	DrawToolbar();
	ImGui::Separator();

	const ImGuiIO& IO = ImGui::GetIO();
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
		&& !IO.WantTextInput
		&& ImGui::IsKeyPressed(ImGuiKey_F2, false))
	{
		RequestRenameSelectedItem();
	}

	if (ImGui::BeginTable("##ContentBrowserLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, IsDrawerMode() ? 250.0f : 230.0f);
		ImGui::TableSetupColumn("Assets", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthFixed, IsDrawerMode() ? 280.0f : 240.0f);

		ImGui::TableNextColumn();
		if (ImGui::BeginChild("##ContentBrowserFolders", ImVec2(0.0f, 0.0f), false))
		{
			DrawDirectoryNode(RootNode);
			PendingRevealPath.clear();
		}
		ImGui::EndChild();

		ImGui::TableNextColumn();
		if (ImGui::BeginChild("##ContentBrowserAssets", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar))
		{
			DrawContentGrid();
		}
		ImGui::EndChild();

		ImGui::TableNextColumn();
		if (ImGui::BeginChild("##ContentBrowserDetails", ImVec2(0.0f, 0.0f), false))
		{
			DrawDetails();
		}
		ImGui::EndChild();

		ImGui::EndTable();
	}

	DrawRenamePopup();
}

void FEditorContentBrowserWidget::DrawToolbar()
{
	{
	const float ItemSpacing = ImGui::GetStyle().ItemSpacing.x;
	if (ImGui::SmallButton("Refresh"))
	{
		Refresh();
	}
	ImGui::SameLine();
	if (ImGui::SmallButton(IsDrawerMode() ? "Window Mode" : "Drawer Mode"))
	{
		PresentationMode = IsDrawerMode() ? EPresentationMode::FloatingWindow : EPresentationMode::Drawer;
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(BackHistory.empty());
	if (ImGui::SmallButton("<"))
	{
		NavigateBack();
	}
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
	{
		ImGui::SetTooltip("Back");
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Up"))
	{
		const std::filesystem::path Parent = CurrentPath.parent_path();
		if (!Parent.empty() && Parent != CurrentPath)
		{
			NavigateTo(Parent);
		}
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Up");
	}
	ImGui::SameLine();
	const float RemainingWidth = ImGui::GetContentRegionAvail().x;
	const float SearchWidth = std::min(220.0f, std::max(120.0f, RemainingWidth * 0.28f));
	const float PathWidth = std::max(120.0f, RemainingWidth - SearchWidth - ItemSpacing);
	ImGui::SetNextItemWidth(PathWidth);
	FString PathText = MakeDisplayPath(CurrentPath);
	char PathBuf[512] = {};
	strncpy_s(PathBuf, PathText.c_str(), _TRUNCATE);
	ImGui::InputText("##ContentBrowserPath", PathBuf, sizeof(PathBuf), ImGuiInputTextFlags_ReadOnly);
	ImGui::SameLine();
	char SearchBuf[128] = {};
	strncpy_s(SearchBuf, SearchFilter.c_str(), _TRUNCATE);
	ImGui::SetNextItemWidth(std::min(SearchWidth, ImGui::GetContentRegionAvail().x));
	if (ImGui::InputTextWithHint("##ContentBrowserSearch", "Search", SearchBuf, sizeof(SearchBuf)))
	{
		SearchFilter = SearchBuf;
	}
	return;
	}

#if 0
	if (ImGui::SmallButton("Refresh"))
	{
		Refresh();
	}
	ImGui::SameLine();
	if (ImGui::SmallButton(IsDrawerMode() ? "Window Mode" : "Drawer Mode"))
	{
		PresentationMode = IsDrawerMode() ? EPresentationMode::FloatingWindow : EPresentationMode::Drawer;
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(BackHistory.empty());
	if (ImGui::SmallButton("?"))
	{
		NavigateBack();
	}
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
	{
		ImGui::SetTooltip("Back");
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("??))
	{
		const std::filesystem::path Parent = CurrentPath.parent_path();
		if (!Parent.empty() && Parent != CurrentPath)
		{
			NavigateTo(Parent);
		}
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Up");
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(std::min(620.0f, ImGui::GetContentRegionAvail().x * 0.62f));
	FString PathText = MakeDisplayPath(CurrentPath);
	char PathBuf[512] = {};
	strncpy_s(PathBuf, PathText.c_str(), _TRUNCATE);
	ImGui::InputText("##ContentBrowserPath", PathBuf, sizeof(PathBuf), ImGuiInputTextFlags_ReadOnly);
	ImGui::SameLine();
	char SearchBuf[128] = {};
	strncpy_s(SearchBuf, SearchFilter.c_str(), _TRUNCATE);
	ImGui::SetNextItemWidth(std::max(120.0f, ImGui::GetContentRegionAvail().x));
	if (ImGui::InputText("Search", SearchBuf, sizeof(SearchBuf)))
	{
		SearchFilter = SearchBuf;
	}
#endif
}

void FEditorContentBrowserWidget::Refresh()
{
	RebuildRootNode();
	RefreshContent();
	bPendingMaterialPreviewCacheClear = true;
	bNeedsRefresh = false;
}

void FEditorContentBrowserWidget::LoadFromSettings()
{
	CurrentPath = ResolveBrowserPath(FEditorSettings::Get().ContentBrowserPath);
	if (!IsPathAllowed(CurrentPath))
	{
		CurrentPath = BrowserRootPaths.empty() ? RootPath : BrowserRootPaths.front();
	}
	PendingRevealPath = CurrentPath;
}

void FEditorContentBrowserWidget::SaveToSettings() const
{
	FEditorSettings::Get().ContentBrowserPath = MakeSavedBrowserPath(CurrentPath);
}

void FEditorContentBrowserWidget::RefreshContent()
{
	CurrentItems = ReadDirectory(CurrentPath);
}

void FEditorContentBrowserWidget::RebuildRootNode()
{
	RootNode = {};
	RootNode.Path = RootPath;
	RootNode.Name = "Project";
	for (const std::filesystem::path& BrowserRoot : BrowserRootPaths)
	{
		std::error_code Ec;
		if (std::filesystem::exists(BrowserRoot, Ec) && std::filesystem::is_directory(BrowserRoot, Ec))
		{
			RootNode.Children.push_back(BuildDirectoryTree(BrowserRoot));
		}
	}
}

FEditorContentBrowserWidget::FDirNode FEditorContentBrowserWidget::BuildDirectoryTree(const std::filesystem::path& DirPath) const
{
	FDirNode Node;
	Node.Path = DirPath;
	Node.Name = FPaths::ToUtf8(DirPath.filename().wstring());
	if (Node.Name.empty())
	{
		Node.Name = "Project";
	}

	std::error_code Ec;
	for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator(DirPath, Ec))
	{
		if (!Entry.is_directory())
		{
			continue;
		}
		Node.Children.push_back(BuildDirectoryTree(Entry.path()));
	}

	std::sort(Node.Children.begin(), Node.Children.end(),
		[](const FDirNode& A, const FDirNode& B)
		{
			return A.Name < B.Name;
		});
	return Node;
}

TArray<FEditorContentBrowserWidget::FContentItem> FEditorContentBrowserWidget::ReadDirectory(const std::filesystem::path& DirPath) const
{
	TArray<FContentItem> Items;
	std::error_code Ec;
	if (IsProjectRootPath(DirPath))
	{
		for (const std::filesystem::path& BrowserRoot : BrowserRootPaths)
		{
			if (std::filesystem::exists(BrowserRoot, Ec) && std::filesystem::is_directory(BrowserRoot, Ec))
			{
				FContentItem Item;
				Item.Path = BrowserRoot;
				Item.Name = FPaths::ToUtf8(BrowserRoot.filename().wstring());
				Item.bIsDirectory = true;
				Items.push_back(Item);
			}
		}
		return Items;
	}
	if (!std::filesystem::exists(DirPath, Ec) || !std::filesystem::is_directory(DirPath, Ec))
	{
		return Items;
	}
	if (!IsPathAllowed(DirPath))
	{
		return Items;
	}

	for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator(DirPath, Ec))
	{
		FContentItem Item;
		Item.Path = Entry.path();
		Item.Name = FPaths::ToUtf8(Entry.path().filename().wstring());
		Item.Extension = ToLower(FPaths::ToUtf8(Entry.path().extension().wstring()));
		Item.bIsDirectory = Entry.is_directory();
		Items.push_back(Item);
	}

	std::sort(Items.begin(), Items.end(),
		[](const FContentItem& A, const FContentItem& B)
		{
			if (A.bIsDirectory != B.bIsDirectory)
			{
				return A.bIsDirectory > B.bIsDirectory;
			}
			return A.Name < B.Name;
		});
	return Items;
}

void FEditorContentBrowserWidget::DrawDirectoryNode(const FDirNode& Node)
{
	ImGuiTreeNodeFlags Flags = Node.Children.empty() ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_OpenOnArrow;
	if (Node.Path == CurrentPath)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}
	if (!PendingRevealPath.empty())
	{
		const std::filesystem::path Relative = PendingRevealPath.lexically_relative(Node.Path);
		if (Node.Path == RootPath || Relative.empty() || !IsParentDirectoryReference(Relative))
		{
			ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		}
	}

	const bool bOpen = ImGui::TreeNodeEx(Node.Name.c_str(), Flags);
	if (ImGui::IsItemClicked())
	{
		NavigateTo(Node.Path);
	}
	if (!bOpen)
	{
		return;
	}
	for (const FDirNode& Child : Node.Children)
	{
		DrawDirectoryNode(Child);
	}
	ImGui::TreePop();
}

void FEditorContentBrowserWidget::DrawContentGrid()
{
	const FString Filter = ToLower(SearchFilter);
	TArray<FContentItem> VisibleItems;
	for (const FContentItem& Item : CurrentItems)
	{
		if (!Filter.empty() && ToLower(Item.Name).find(Filter) == FString::npos)
		{
			continue;
		}
		VisibleItems.push_back(Item);
	}

	const ImVec2 Available = ImGui::GetContentRegionAvail();
	constexpr float BottomBarHeight = 30.0f;
	const float GridHeight = std::max(1.0f, Available.y - BottomBarHeight);
	if (ImGui::BeginChild("##ContentBrowserAssetGridScroll", ImVec2(0.0f, GridHeight), false, ImGuiWindowFlags_HorizontalScrollbar))
	{
		const float ContentWidth = ImGui::GetContentRegionAvail().x;
		const ImVec2 Tile(TileSize, TileSize + 44.0f);
		constexpr float TileGap = 14.0f;
		const int32 Columns = std::max(1, static_cast<int32>((ContentWidth + TileGap) / (Tile.x + TileGap)));

		MaterialPreviewBuildsThisFrame = 0;
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(TileGap, TileGap));
		bool bAnyTileHovered = false;
		for (int32 Index = 0; Index < static_cast<int32>(VisibleItems.size()); ++Index)
		{
			if (Index > 0 && Index % Columns != 0)
			{
				ImGui::SameLine(0.0f, TileGap);
			}
			DrawContentTile(VisibleItems[Index], Tile);
			bAnyTileHovered |= ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);
		}
		ImGui::PopStyleVar();

		if (!bAnyTileHovered
			&& ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)
			&& ImGui::IsMouseClicked(ImGuiMouseButton_Right))
		{
			bOpenContentContextMenu = true;
			bContentContextMenuHasSelection = false;
		}

		if (bOpenContentContextMenu)
		{
			ImGui::OpenPopup("##ContentBrowserContextMenu");
			bOpenContentContextMenu = false;
		}
		if (ImGui::BeginPopup("##ContentBrowserContextMenu"))
		{
			DrawContentContextMenu(bContentContextMenuHasSelection);
			ImGui::EndPopup();
		}
	}
	ImGui::EndChild();

	const float SliderWidth = 112.0f;
	const float StartX = std::max(ImGui::GetCursorPosX(), ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - SliderWidth - 34.0f);
	ImGui::SetCursorPosX(StartX);
	ImGui::TextDisabled("Size");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(SliderWidth);
	ImGui::SliderFloat("##ContentTileSize", &TileSize, 48.0f, 128.0f, "%.0f");
}

void FEditorContentBrowserWidget::DrawContentTile(const FContentItem& Item, const ImVec2& TileSize)
{
	ImGui::PushID(FPaths::ToUtf8(Item.Path.wstring()).c_str());
	const bool bSelected = SelectedPath == Item.Path;
	if (ImGui::Selectable("##ContentTile", bSelected, 0, TileSize))
	{
		SelectedPath = Item.Path;
	}

	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		SelectedPath = Item.Path;
		bOpenContentContextMenu = true;
		bContentContextMenuHasSelection = true;
	}

	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImU32 Bg = ImGui::GetColorU32(bSelected ? ImVec4(0.19f, 0.28f, 0.43f, 1.0f) : ImVec4(0.13f, 0.15f, 0.18f, 1.0f));
	const ImU32 Border = ImGui::GetColorU32(bSelected ? ImVec4(0.36f, 0.58f, 0.88f, 1.0f) : ImVec4(0.25f, 0.28f, 0.33f, 1.0f));
	DrawList->AddRectFilled(Min, Max, Bg, 6.0f);
	DrawList->AddRect(Min, Max, Border, 6.0f);

	const ImVec2 IconMin(Min.x + 10.0f, Min.y + 8.0f);
	const ImVec2 IconMax(Max.x - 10.0f, Min.y + TileSize.y - 46.0f);
	ID3D11ShaderResourceView* PreviewSRV = nullptr;
	if (!Item.bIsDirectory)
	{
		if (IsPreviewableImage(Item.Extension))
		{
			PreviewSRV = GetImagePreviewSRV(Item);
		}
	}

	if (Item.bIsDirectory)
	{
		DrawFolderIcon(DrawList, IconMin, IconMax, GetItemColor(Item));
	}
	else if (PreviewSRV)
	{
		DrawList->AddRectFilled(IconMin, IconMax, ImGui::GetColorU32(ImVec4(0.05f, 0.06f, 0.07f, 1.0f)), 5.0f);
		DrawList->AddImage(reinterpret_cast<ImTextureID>(PreviewSRV), IconMin, IconMax);
		DrawList->AddRect(IconMin, IconMax, ImGui::GetColorU32(ImVec4(0.25f, 0.28f, 0.33f, 0.75f)), 5.0f);
	}
	else
	{
		DrawList->AddRectFilled(IconMin, IconMax, GetItemColor(Item), 5.0f);
		if (IsMaterialAsset(Item.Extension))
		{
			const ImVec2 Center((IconMin.x + IconMax.x) * 0.5f, (IconMin.y + IconMax.y) * 0.5f - 4.0f);
			const float Radius = std::max(10.0f, std::min(IconMax.x - IconMin.x, IconMax.y - IconMin.y) * 0.28f);
			DrawList->AddCircleFilled(Center, Radius, ImGui::GetColorU32(ImVec4(0.18f, 0.20f, 0.24f, 0.82f)), 32);
			DrawList->AddCircleFilled(ImVec2(Center.x - Radius * 0.32f, Center.y - Radius * 0.28f), Radius * 0.36f,
				ImGui::GetColorU32(ImVec4(0.95f, 0.78f, 0.42f, 0.85f)), 20);
			DrawList->AddCircleFilled(ImVec2(Center.x + Radius * 0.22f, Center.y + Radius * 0.12f), Radius * 0.42f,
				ImGui::GetColorU32(ImVec4(0.45f, 0.62f, 0.88f, 0.78f)), 20);
			const char* Kind = Item.Extension == ".matinst" ? "MI" : "MAT";
			const ImVec2 TextSize = ImGui::CalcTextSize(Kind);
			DrawList->AddText(ImVec2(Center.x - TextSize.x * 0.5f, IconMax.y - 22.0f),
				ImGui::GetColorU32(ImVec4(0.96f, 0.97f, 0.99f, 1.0f)), Kind);
		}
		else if (IsCurveAsset(Item.Path))
		{
			const float Width = IconMax.x - IconMin.x;
			const float Height = IconMax.y - IconMin.y;
			const ImU32 LineColor = ImGui::GetColorU32(ImVec4(0.98f, 0.93f, 0.48f, 1.0f));
			ImVec2 Prev(IconMin.x + Width * 0.15f, IconMax.y - Height * 0.20f);
			for (int32 Step = 1; Step <= 5; ++Step)
			{
				const float Alpha = static_cast<float>(Step) / 5.0f;
				const float X = IconMin.x + Width * (0.15f + Alpha * 0.70f);
				const float Y = IconMax.y - Height * (0.20f + Alpha * Alpha * 0.62f);
				const ImVec2 Next(X, Y);
				DrawList->AddLine(Prev, Next, LineColor, 3.0f);
				Prev = Next;
			}
			const char* Kind = "CURVE";
			const ImVec2 TextSize = ImGui::CalcTextSize(Kind);
			DrawList->AddText(ImVec2((IconMin.x + IconMax.x - TextSize.x) * 0.5f, IconMax.y - 22.0f),
				ImGui::GetColorU32(ImVec4(0.96f, 0.97f, 0.99f, 1.0f)), Kind);
		}
	}

	FString Label = Item.Name;
	auto Ellipsize = [](FString Text, float MaxWidth)
	{
		if (ImGui::CalcTextSize(Text.c_str()).x <= MaxWidth)
		{
			return Text;
		}
		while (!Text.empty() && ImGui::CalcTextSize((Text + "...").c_str()).x > MaxWidth)
		{
			Text.pop_back();
		}
		return Text + "...";
	};

	const float LabelWidth = TileSize.x - 12.0f;
	Label = Ellipsize(Label, LabelWidth);
	FString ExtLine = "file";
	if (Item.bIsDirectory)
	{
		ExtLine = "folder";
	}
	else if (!Item.Extension.empty())
	{
		ExtLine = Item.Extension;
	}
	if (IsCurveAsset(Item.Path))
	{
		ExtLine = ".curve";
	}
	DrawList->AddText(ImVec2(Min.x + 6.0f, Max.y - 35.0f), ImGui::GetColorU32(ImGuiCol_Text), Label.c_str());
	DrawList->AddText(ImVec2(Min.x + 6.0f, Max.y - 18.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), ExtLine.c_str());

	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
	{
		if (Item.bIsDirectory)
		{
			NavigateTo(Item.Path);
		}
		else if (IsMaterialAsset(Item.Extension))
		{
			if (UMaterialInterface* Material = ResolveMaterialAsset(Item.Path))
			{
				EditorEngine->GetMainPanel().OpenMaterialAsset(Material);
			}
		}
		else if (IsCurveAsset(Item.Path))
		{
			EditorEngine->GetMainPanel().OpenCurveAsset(MakeRelativeProjectPath(Item.Path));
		}
		else if (Item.Extension == ".scene")
		{
			EditorEngine->GetMainPanel().GetSceneWidget().LoadSceneFromFilePath(FPaths::ToUtf8(Item.Path.wstring()));
		}
		else if (IsPrefabAsset(Item.Extension))
		{
			EditorEngine->GetMainPanel().PushFooterLog("Prefab selected. Drag to viewport or right-click to spawn.");
		}
		else
		{
			ShellExecuteW(nullptr, L"open", Item.Path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
		}
	}

	if (!Item.bIsDirectory && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
	{
		const FString PayloadPath = FPaths::ToUtf8(Item.Path.wstring());
		const FString PayloadType = GetPayloadType(Item);
		ActiveDragPayloadPath = PayloadPath;
		ActiveDragPayloadType = PayloadType;
		ImGui::SetDragDropPayload(PayloadType.c_str(), PayloadPath.c_str(), PayloadPath.size() + 1);
		ImGui::TextUnformatted(Item.Name.c_str());
		ImGui::TextDisabled("%s", PayloadPath.c_str());
		ImGui::EndDragDropSource();
	}
	ImGui::PopID();
}

void FEditorContentBrowserWidget::DrawContentContextMenu(bool bHasSelectedItem)
{
	const bool bCanCreateHere = !IsProjectRootPath(CurrentPath) && IsPathAllowed(CurrentPath);
	ImGui::BeginDisabled(!bCanCreateHere);
	if (ImGui::BeginMenu("Create"))
	{
		if (ImGui::MenuItem("Folder"))
		{
			CreateFolder();
			ImGui::CloseCurrentPopup();
		}
		if (ImGui::MenuItem("Text File"))
		{
			CreateTextFile();
			ImGui::CloseCurrentPopup();
		}
		if (ImGui::MenuItem("Lua Script"))
		{
			CreateLuaScriptFile();
			ImGui::CloseCurrentPopup();
		}
		if (ImGui::MenuItem("Material"))
		{
			CreateMaterialAsset();
			ImGui::CloseCurrentPopup();
		}
		if (ImGui::MenuItem("Curve"))
		{
			CreateCurveAsset();
			ImGui::CloseCurrentPopup();
		}
		if (ImGui::MenuItem("Scene"))
		{
			CreateSceneAsset();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndMenu();
	}
	ImGui::EndDisabled();

	ImGui::Separator();
	ImGui::BeginDisabled(!bHasSelectedItem || SelectedPath.empty());
	const FString SelectedExtension = ToLower(FPaths::ToUtf8(SelectedPath.extension().wstring()));
	if (ImGui::MenuItem("Spawn Prefab at Origin", nullptr, false, IsPrefabAsset(SelectedExtension)))
	{
		EditorEngine->GetMainPanel().SpawnPrefabAtOrigin(FPaths::ToUtf8(SelectedPath.wstring()));
		ImGui::CloseCurrentPopup();
	}
	if (ImGui::MenuItem("Rename", "F2"))
	{
		RequestRenameSelectedItem();
		ImGui::CloseCurrentPopup();
	}
	if (ImGui::MenuItem("Delete", "Del"))
	{
		DeleteSelectedItem();
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndDisabled();
}

bool FEditorContentBrowserWidget::CreateFolder()
{
	std::error_code Ec;
	const std::filesystem::path NewPath = MakeUniquePath(CurrentPath / L"New Folder");
	std::filesystem::create_directories(NewPath, Ec);
	if (Ec)
	{
		return false;
	}
	SelectedPath = NewPath;
	Refresh();
	return true;
}

bool FEditorContentBrowserWidget::CreateTextFile()
{
	const std::filesystem::path NewPath = MakeUniquePath(CurrentPath / L"New Text.txt");
	std::ofstream OutFile(NewPath, std::ios::out | std::ios::trunc);
	if (!OutFile.is_open())
	{
		return false;
	}
	OutFile.close();
	SelectedPath = NewPath;
	RefreshContent();
	return true;
}

bool FEditorContentBrowserWidget::CreateLuaScriptFile()
{
	const std::filesystem::path TargetDir = ResolveLuaScriptCreateDirectory();
	std::error_code CreateDirEc;
	std::filesystem::create_directories(TargetDir, CreateDirEc);
	if (CreateDirEc)
	{
		return false;
	}

	const std::filesystem::path NewPath = MakeUniquePath(TargetDir / L"New Script.lua");
	const FString RelativePath = MakeRelativeProjectPath(NewPath);
	if (!FScriptManager::Get().CreateScript(FName(RelativePath.c_str())))
	{
		return false;
	}

	SelectedPath = NewPath;
	if (CurrentPath.lexically_normal() != TargetDir.lexically_normal())
	{
		NavigateTo(TargetDir);
	}
	else
	{
		RefreshContent();
	}
	return true;
}

bool FEditorContentBrowserWidget::CreateMaterialAsset()
{
	const std::filesystem::path NewPath = MakeUniquePath(CurrentPath / L"New Material.mat");
	const FString RelativePath = MakeRelativeProjectPath(NewPath);
	const FString MaterialName = FPaths::ToUtf8(NewPath.stem().wstring());

	UMaterial* Material = FResourceManager::Get().GetOrCreateMaterial(MaterialName, RelativePath, EMaterialShaderType::SurfaceLit);
	if (!Material)
	{
		return false;
	}

	Material->MaterialData.Name = MaterialName;
	Material->MaterialData.bHasDiffuseTexture = false;
	Material->MaterialData.bHasAmbientTexture = false;
	Material->MaterialData.bHasSpecularTexture = false;
	Material->MaterialData.bHasBumpTexture = false;
	Material->SetParam("AmbientColor", FMaterialParamValue(Material->MaterialData.AmbientColor));
	Material->SetParam("DiffuseColor", FMaterialParamValue(Material->MaterialData.DiffuseColor));
	Material->SetParam("SpecularColor", FMaterialParamValue(Material->MaterialData.SpecularColor));
	Material->SetParam("EmissiveColor", FMaterialParamValue(Material->MaterialData.EmissiveColor));
	Material->SetParam("Shininess", FMaterialParamValue(Material->MaterialData.Shininess));
	Material->SetParam("Opacity", FMaterialParamValue(Material->MaterialData.Opacity));
	Material->SetParam("ScrollUV", FMaterialParamValue(FVector2(0.0f, 0.0f)));
	Material->SetParam("bHasDiffuseMap", FMaterialParamValue(false));
	Material->SetParam("bHasSpecularMap", FMaterialParamValue(false));
	Material->SetParam("bHasAmbientMap", FMaterialParamValue(false));
	Material->SetParam("bHasEmissiveMap", FMaterialParamValue(false));
	Material->SetParam("bHasBumpMap", FMaterialParamValue(false));
	if (UTexture* DefaultWhite = FResourceManager::Get().GetTexture("DefaultWhite"))
	{
		Material->SetParam("DiffuseMap", FMaterialParamValue(DefaultWhite));
		Material->SetParam("AmbientMap", FMaterialParamValue(DefaultWhite));
		Material->SetParam("SpecularMap", FMaterialParamValue(DefaultWhite));
		Material->SetParam("EmissiveMap", FMaterialParamValue(DefaultWhite));
		Material->SetParam("BumpMap", FMaterialParamValue(DefaultWhite));
	}

	if (!FResourceManager::Get().SerializeMaterial(RelativePath, Material))
	{
		return false;
	}

	SelectedPath = NewPath;
	RefreshContent();
	return true;
}

bool FEditorContentBrowserWidget::CreateCurveAsset()
{
	const std::filesystem::path NewPath = MakeUniquePath(CurrentPath / L"New Curve.curve.json");
	const FString RelativePath = MakeRelativeProjectPath(NewPath);

	UCurveFloatAsset* Curve = UObjectManager::Get().CreateObject<UCurveFloatAsset>();
	if (!Curve)
	{
		return false;
	}

	Curve->SetAssetPath(RelativePath);

	FCurveKey StartKey;
	StartKey.Time = 0.0f;
	StartKey.Value = 0.0f;
	StartKey.InterpMode = ECurveInterpMode::Linear;

	FCurveKey EndKey;
	EndKey.Time = 1.0f;
	EndKey.Value = 1.0f;
	EndKey.InterpMode = ECurveInterpMode::Linear;

	FFloatCurve& FloatCurve = Curve->GetMutableCurve();
	FloatCurve.Keys.clear();
	FloatCurve.Keys.push_back(StartKey);
	FloatCurve.Keys.push_back(EndKey);
	FloatCurve.SortKeys();

	if (!FResourceManager::Get().SaveCurve(RelativePath, Curve))
	{
		return false;
	}

	SelectedPath = NewPath;
	RefreshContent();
	return true;
}

bool FEditorContentBrowserWidget::CreateSceneAsset()
{
	if (!EditorEngine)
	{
		return false;
	}

	const std::filesystem::path NewPath = MakeUniquePath(CurrentPath / L"New Scene.Scene");
	if (!EditorEngine->CreateDefaultSceneAsset(FPaths::ToUtf8(NewPath.wstring())))
	{
		return false;
	}

	SelectedPath = NewPath;
	RefreshContent();
	return true;
}

bool FEditorContentBrowserWidget::DeleteSelectedItem()
{
	if (SelectedPath.empty())
	{
		return false;
	}
	if (SelectedPath == CurrentPath || !IsPathAllowed(SelectedPath))
	{
		return false;
	}
	for (const std::filesystem::path& BrowserRoot : BrowserRootPaths)
	{
		if (SelectedPath.lexically_normal() == BrowserRoot.lexically_normal())
		{
			return false;
		}
	}

	const std::wstring Message = L"Delete selected content?\n\n" + SelectedPath.wstring();
	const int Result = MessageBoxW(nullptr, Message.c_str(), L"Delete Content", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);
	if (Result != IDYES)
	{
		return false;
	}

	std::error_code Ec;
	if (std::filesystem::is_directory(SelectedPath, Ec))
	{
		std::filesystem::remove_all(SelectedPath, Ec);
	}
	else
	{
		std::filesystem::remove(SelectedPath, Ec);
	}
	if (Ec)
	{
		return false;
	}

	SelectedPath.clear();
	Refresh();
	return true;
}

void FEditorContentBrowserWidget::RequestRenameSelectedItem()
{
	if (SelectedPath.empty() || !IsPathAllowed(SelectedPath))
	{
		return;
	}
	for (const std::filesystem::path& BrowserRoot : BrowserRootPaths)
	{
		if (SelectedPath.lexically_normal() == BrowserRoot.lexically_normal())
		{
			return;
		}
	}

	RenameSourcePath = SelectedPath;
	const FString CurrentName = FPaths::ToUtf8(SelectedPath.filename().wstring());
	strncpy_s(RenameBuffer, CurrentName.c_str(), _TRUNCATE);
	bRenamePopupRequested = true;
}

bool FEditorContentBrowserWidget::CommitRename()
{
	if (RenameSourcePath.empty() || RenameBuffer[0] == '\0')
	{
		return false;
	}

	std::filesystem::path NewName(FPaths::ToWide(FString(RenameBuffer)));
	if (NewName.has_parent_path() || NewName.is_absolute())
	{
		return false;
	}

	std::error_code Ec;
	if (!std::filesystem::exists(RenameSourcePath, Ec))
	{
		return false;
	}

	const bool bIsDirectory = std::filesystem::is_directory(RenameSourcePath, Ec);
	if (!bIsDirectory && NewName.extension().empty())
	{
		NewName += RenameSourcePath.extension();
	}

	const std::filesystem::path TargetPath = (RenameSourcePath.parent_path() / NewName).lexically_normal();
	if (!IsPathAllowed(TargetPath) || TargetPath == RenameSourcePath)
	{
		return false;
	}
	if (std::filesystem::exists(TargetPath, Ec))
	{
		return false;
	}

	std::filesystem::rename(RenameSourcePath, TargetPath, Ec);
	if (Ec)
	{
		return false;
	}

	SelectedPath = TargetPath;
	RenameSourcePath.clear();
	Refresh();
	return true;
}

void FEditorContentBrowserWidget::DrawRenamePopup()
{
	if (bRenamePopupRequested)
	{
		ImGui::OpenPopup("Rename Content");
		bRenamePopupRequested = false;
	}

	if (ImGui::BeginPopupModal("Rename Content", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Rename selected content");
		ImGui::TextDisabled("References are not remapped yet.");
		ImGui::SetNextItemWidth(320.0f);
		const bool bEnter = ImGui::InputText("##RenameContentInput", RenameBuffer, sizeof(RenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::Spacing();
		if (ImGui::Button("Rename") || bEnter)
		{
			if (CommitRename())
			{
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			RenameSourcePath.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

std::filesystem::path FEditorContentBrowserWidget::MakeUniquePath(const std::filesystem::path& DesiredPath) const
{
	std::filesystem::path Candidate = DesiredPath;
	std::error_code Ec;
	if (!std::filesystem::exists(Candidate, Ec))
	{
		return Candidate;
	}

	const std::filesystem::path Parent = DesiredPath.parent_path();
	const std::wstring Stem = DesiredPath.stem().wstring();
	const std::wstring Extension = DesiredPath.extension().wstring();
	for (int32 Index = 1; Index < 10000; ++Index)
	{
		Candidate = Parent / (Stem + L" " + std::to_wstring(Index) + Extension);
		if (!std::filesystem::exists(Candidate, Ec))
		{
			return Candidate;
		}
	}
	return DesiredPath;
}

void FEditorContentBrowserWidget::DrawFolderIcon(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color) const
{
	const float Width = Max.x - Min.x;
	const float Height = Max.y - Min.y;
	const ImVec2 TabMin(Min.x + Width * 0.08f, Min.y + Height * 0.14f);
	const ImVec2 TabMax(Min.x + Width * 0.44f, Min.y + Height * 0.34f);
	const ImVec2 BodyMin(Min.x + Width * 0.06f, Min.y + Height * 0.26f);
	const ImVec2 BodyMax(Min.x + Width * 0.94f, Min.y + Height * 0.86f);
	const ImU32 Shadow = ImGui::GetColorU32(ImVec4(0.04f, 0.05f, 0.06f, 0.42f));
	const ImU32 Highlight = ImGui::GetColorU32(ImVec4(1.0f, 0.92f, 0.60f, 0.18f));

	DrawList->AddRectFilled(ImVec2(BodyMin.x + 2.0f, BodyMin.y + 3.0f), ImVec2(BodyMax.x + 2.0f, BodyMax.y + 3.0f), Shadow, 5.0f);
	DrawList->AddRectFilled(TabMin, TabMax, Color, 4.0f);
	DrawList->AddRectFilled(BodyMin, BodyMax, Color, 5.0f);
	DrawList->AddRectFilled(ImVec2(BodyMin.x + 4.0f, BodyMin.y + 5.0f), ImVec2(BodyMax.x - 4.0f, BodyMin.y + 11.0f), Highlight, 4.0f);
	DrawList->AddRect(BodyMin, BodyMax, ImGui::GetColorU32(ImVec4(0.20f, 0.18f, 0.10f, 0.45f)), 5.0f);
}

void FEditorContentBrowserWidget::DrawDetails()
{
	ImGui::TextUnformatted("Details");
	ImGui::Separator();
	if (SelectedPath.empty())
	{
		ImGui::TextDisabled("No asset selected.");
		return;
	}

	const std::filesystem::path Filename = SelectedPath.filename();
	const FString PathText = FPaths::ToUtf8(SelectedPath.wstring());
	ImGui::TextWrapped("%s", FPaths::ToUtf8(Filename.wstring()).c_str());
	ImGui::Spacing();
	ImGui::TextDisabled("Path");
	ImGui::TextWrapped("%s", PathText.c_str());

	std::error_code Ec;
	if (std::filesystem::exists(SelectedPath, Ec) && !std::filesystem::is_directory(SelectedPath, Ec))
	{
		ImGui::Spacing();
		ImGui::Text("Size: %.2f KB", static_cast<double>(std::filesystem::file_size(SelectedPath, Ec)) / 1024.0);
		DrawAssetPreview();
	}
}

void FEditorContentBrowserWidget::DrawAssetPreview()
{
	const FString Extension = ToLower(FPaths::ToUtf8(SelectedPath.extension().wstring()));
	const FString RelativePath = MakeRelativeProjectPath(SelectedPath);
	if (IsPreviewableImage(Extension))
	{
		if (UTexture* Texture = FResourceManager::Get().LoadTexture(RelativePath))
		{
			if (ID3D11ShaderResourceView* SRV = Texture->GetSRV())
			{
				ImGui::Spacing();
				ImGui::TextDisabled("Preview");
				const float Width = std::min(ImGui::GetContentRegionAvail().x, 220.0f);
				ImGui::Image(reinterpret_cast<ImTextureID>(SRV), ImVec2(Width, Width));
			}
		}
		return;
	}

	if (IsMaterialAsset(Extension))
	{
		UMaterialInterface* Material = FResourceManager::Get().GetMaterialInterface(RelativePath);
		if (!Material)
		{
			Material = FResourceManager::Get().GetMaterialInterface(FPaths::Normalize(FPaths::ToUtf8(SelectedPath.wstring())));
		}
		if (!Material)
		{
			Material = FResourceManager::Get().GetMaterialInterface(FPaths::ToUtf8(SelectedPath.stem().wstring()));
		}
		ImGui::Spacing();
		ImGui::TextDisabled("Material");
		if (!Material)
		{
			ImGui::TextWrapped("Not loaded in ResourceManager.");
			return;
		}

		ImGui::TextWrapped("%s", Material->GetName().c_str());
		TMap<FString, FMaterialParamValue> Params;
		Material->GatherAllParams(Params);

		ImGui::Spacing();
		ImGui::TextDisabled("Color Parameters");
		int32 ColorCount = 0;
		for (const auto& [ParamName, ParamValue] : Params)
		{
			if (ParamValue.Type == EMaterialParamType::Vector3 && std::holds_alternative<FVector>(ParamValue.Value))
			{
				const FVector& Color = std::get<FVector>(ParamValue.Value);
				ImGui::ColorButton(ParamName.c_str(), ImVec4(Color.X, Color.Y, Color.Z, 1.0f), ImGuiColorEditFlags_NoTooltip, ImVec2(38.0f, 20.0f));
				ImGui::SameLine();
				ImGui::TextUnformatted(ParamName.c_str());
				++ColorCount;
			}
		}
		if (ColorCount == 0)
		{
			ImGui::TextDisabled("No color parameters.");
		}

		ImGui::Spacing();
		ImGui::TextDisabled("Texture Parameters");
		int32 TextureCount = 0;
		for (const auto& [ParamName, ParamValue] : Params)
		{
			if (ParamValue.Type == EMaterialParamType::Texture && std::holds_alternative<UTexture*>(ParamValue.Value))
			{
				UTexture* Texture = std::get<UTexture*>(ParamValue.Value);
				if (Texture && Texture->GetSRV())
				{
					ImGui::TextDisabled("%s", ParamName.c_str());
					ImGui::Image(reinterpret_cast<ImTextureID>(Texture->GetSRV()), ImVec2(72.0f, 72.0f));
					++TextureCount;
				}
			}
		}
		if (TextureCount == 0)
		{
			ImGui::TextDisabled("No texture parameter preview.");
		}
		return;
	}

	if (IsCurveAsset(SelectedPath))
	{
		UCurveFloatAsset* Curve = FResourceManager::Get().LoadCurve(RelativePath);
		ImGui::Spacing();
		ImGui::TextDisabled("Float Curve");
		if (!Curve)
		{
			ImGui::TextWrapped("Not loaded in ResourceManager.");
			return;
		}

		const FFloatCurve& FloatCurve = Curve->GetCurve();
		ImGui::Text("Keys: %d", static_cast<int32>(FloatCurve.Keys.size()));
		ImGui::Text("Range: %.3f - %.3f", FloatCurve.GetStartTime(), FloatCurve.GetEndTime());
		ImGui::Text("Value at 0.0: %.3f", Curve->Evaluate(0.0f));
		ImGui::Text("Value at 1.0: %.3f", Curve->Evaluate(1.0f));
		return;
	}

	if (IsPrefabAsset(Extension))
	{
		ImGui::Spacing();
		ImGui::TextDisabled("Prefab Template");
		ImGui::TextWrapped("Spawns a normal independent actor. Scene saves the spawned actor, not a prefab link.");
		ImGui::Spacing();
		if (ImGui::Button("Spawn at Origin"))
		{
			EditorEngine->GetMainPanel().SpawnPrefabAtOrigin(RelativePath);
		}
	}
}

ID3D11ShaderResourceView* FEditorContentBrowserWidget::GetImagePreviewSRV(const FContentItem& Item)
{
	if (!IsPreviewableImage(Item.Extension))
	{
		return nullptr;
	}

	UTexture* Texture = FResourceManager::Get().GetTexture(MakeRelativeProjectPath(Item.Path));
	return Texture ? Texture->GetSRV() : nullptr;
}

ID3D11ShaderResourceView* FEditorContentBrowserWidget::GetMaterialPreviewSRV(const FContentItem& Item, uint32 Width, uint32 Height, bool bHighPriority)
{
	if (!EditorEngine || !IsMaterialAsset(Item.Extension) || Width == 0 || Height == 0)
	{
		return nullptr;
	}

	const FString RelativePath = MakeRelativeProjectPath(Item.Path);
	const FString CacheKey = RelativePath + "#" + std::to_string(Width) + "x" + std::to_string(Height);
	auto Found = MaterialPreviewCache.find(CacheKey);
	if (Found != MaterialPreviewCache.end())
	{
		return Found->second.SRV.Get();
	}

	if (!bHighPriority && MaterialPreviewBuildsThisFrame >= 1)
	{
		return nullptr;
	}

	UMaterialInterface* Material = ResolveMaterialAsset(Item.Path);
	if (!Material)
	{
		return nullptr;
	}

	if (MaterialPreviewMesh == nullptr)
	{
		MaterialPreviewMesh = FResourceManager::Get().LoadStaticMesh("Asset\\Mesh\\PreviewSphere.obj");
	}
	if (!MaterialPreviewMesh || !MaterialPreviewMesh->HasValidMeshData())
	{
		return nullptr;
	}

	FEditorRenderPipeline* RenderPipeline = EditorEngine->GetEditorRenderPipeline();
	if (!RenderPipeline)
	{
		return nullptr;
	}

	++MaterialPreviewBuildsThisFrame;
	ID3D11ShaderResourceView* PreviewSRV = RenderPipeline->RenderMaterialPreview(
		EditorEngine->GetRenderer(),
		MaterialPreviewMesh,
		Material,
		Width,
		Height,
		0.8f,
		0.25f,
		4.0f);

	FMaterialPreviewSnapshot Snapshot;
	if (!CapturePreviewSnapshot(PreviewSRV, Snapshot, Width, Height))
	{
		return nullptr;
	}

	FMaterialPreviewSnapshot& CachedSnapshot = MaterialPreviewCache[CacheKey];
	CachedSnapshot = Snapshot;
	return CachedSnapshot.SRV.Get();
}

bool FEditorContentBrowserWidget::CapturePreviewSnapshot(ID3D11ShaderResourceView* SourceSRV, FMaterialPreviewSnapshot& OutSnapshot, uint32 Width, uint32 Height)
{
	if (!EditorEngine || !SourceSRV)
	{
		return false;
	}

	ID3D11Device* Device = EditorEngine->GetRenderer().GetFD3DDevice().GetDevice();
	ID3D11DeviceContext* Context = EditorEngine->GetRenderer().GetFD3DDevice().GetDeviceContext();
	if (!Device || !Context)
	{
		return false;
	}

	TComPtr<ID3D11Resource> SourceResource;
	SourceSRV->GetResource(SourceResource.GetAddressOf());
	if (!SourceResource)
	{
		return false;
	}

	TComPtr<ID3D11Texture2D> SourceTexture;
	if (FAILED(SourceResource.As(&SourceTexture)) || !SourceTexture)
	{
		return false;
	}

	D3D11_TEXTURE2D_DESC Desc = {};
	SourceTexture->GetDesc(&Desc);
	Desc.Width = Width;
	Desc.Height = Height;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	Desc.CPUAccessFlags = 0;
	Desc.MiscFlags = 0;
	Desc.Usage = D3D11_USAGE_DEFAULT;

	OutSnapshot.Texture.Reset();
	OutSnapshot.SRV.Reset();
	if (FAILED(Device->CreateTexture2D(&Desc, nullptr, OutSnapshot.Texture.GetAddressOf())))
	{
		return false;
	}

	Context->CopyResource(OutSnapshot.Texture.Get(), SourceTexture.Get());
	if (FAILED(Device->CreateShaderResourceView(OutSnapshot.Texture.Get(), nullptr, OutSnapshot.SRV.GetAddressOf())))
	{
		OutSnapshot.Texture.Reset();
		return false;
	}

	OutSnapshot.Width = Width;
	OutSnapshot.Height = Height;
	return true;
}

UMaterialInterface* FEditorContentBrowserWidget::ResolveMaterialAsset(const std::filesystem::path& Path)
{
	const FString RelativePath = MakeRelativeProjectPath(Path);
	UMaterialInterface* Material = FResourceManager::Get().GetMaterialInterface(RelativePath);
	if (Material)
	{
		return Material;
	}
	const FString AbsolutePath = FPaths::Normalize(FPaths::ToUtf8(Path.wstring()));
	Material = FResourceManager::Get().GetMaterialInterface(AbsolutePath);
	if (Material)
	{
		return Material;
	}

	const FString Extension = ToLower(FPaths::ToUtf8(Path.extension().wstring()));
	if (Extension == ".mat" || Extension == ".matinst")
	{
		FResourceManager::Get().DeserializeMaterial(RelativePath);
	}

	Material = FResourceManager::Get().GetMaterialInterface(RelativePath);
	if (Material)
	{
		return Material;
	}
	Material = FResourceManager::Get().GetMaterialInterface(AbsolutePath);
	if (Material)
	{
		return Material;
	}
	return FResourceManager::Get().GetMaterialInterface(FPaths::ToUtf8(Path.stem().wstring()));
}

void FEditorContentBrowserWidget::NavigateTo(const std::filesystem::path& Path)
{
	NavigateTo(Path, true);
}

void FEditorContentBrowserWidget::NavigateTo(const std::filesystem::path& Path, bool bAddHistory)
{
	const std::filesystem::path Normalized = Path.lexically_normal();
	std::error_code Ec;
	if (!std::filesystem::exists(Normalized, Ec) || !std::filesystem::is_directory(Normalized, Ec))
	{
		return;
	}
	if (!IsProjectRootPath(Normalized) && !IsPathAllowed(Normalized))
	{
		return;
	}
	if (bAddHistory && CurrentPath != Normalized)
	{
		BackHistory.push_back(CurrentPath);
	}
	CurrentPath = Normalized;
	PendingRevealPath = CurrentPath;
	SelectedPath.clear();
	RefreshContent();
	SaveToSettings();
}

void FEditorContentBrowserWidget::NavigateBack()
{
	while (!BackHistory.empty())
	{
		const std::filesystem::path Previous = BackHistory.back();
		BackHistory.pop_back();
		std::error_code Ec;
		if (std::filesystem::exists(Previous, Ec) && std::filesystem::is_directory(Previous, Ec))
		{
			NavigateTo(Previous, false);
			return;
		}
	}
}

FString FEditorContentBrowserWidget::MakeDisplayPath(const std::filesystem::path& Path) const
{
	const std::filesystem::path Relative = Path.lexically_relative(RootPath);
	if (!Relative.empty() && !IsParentDirectoryReference(Relative))
	{
		return FString("Project/") + FPaths::ToUtf8(Relative.generic_wstring());
	}
	return FPaths::ToUtf8(Path.wstring());
}

FString FEditorContentBrowserWidget::GetPayloadType(const FContentItem& Item) const
{
	if (Item.Extension == ".obj" || Item.Extension == ".bin")
	{
		return "ObjectContentItem";
	}
	if (Item.Extension == ".mat" || Item.Extension == ".matinst")
	{
		return "MaterialContentItem";
	}
	if (IsCurveAsset(Item.Path))
	{
		return "CurveContentItem";
	}
	if (Item.Extension == ".prefab")
	{
		return "PrefabContentItem";
	}
	if (Item.Extension == ".lua")
	{
		return "LuaScriptContentItem";
	}
	if (Item.Extension == ".rml")
	{
		return "RMLContentItem";
	}
	if (Item.Extension == ".png")
	{
		return "PNGElement";
	}
	if (Item.Extension == ".jpg" || Item.Extension == ".jpeg" || Item.Extension == ".dds")
	{
		return "TextureContentItem";
	}
	return "ContentBrowserPath";
}

ImU32 FEditorContentBrowserWidget::GetItemColor(const FContentItem& Item) const
{
	if (Item.bIsDirectory)
	{
		return ImGui::GetColorU32(ImVec4(0.82f, 0.61f, 0.22f, 1.0f));
	}
	if (Item.Extension == ".scene")
	{
		return ImGui::GetColorU32(ImVec4(0.26f, 0.52f, 0.78f, 1.0f));
	}
	if (Item.Extension == ".obj" || Item.Extension == ".bin")
	{
		return ImGui::GetColorU32(ImVec4(0.40f, 0.65f, 0.54f, 1.0f));
	}
	if (Item.Extension == ".mat" || Item.Extension == ".matinst")
	{
		return ImGui::GetColorU32(ImVec4(0.65f, 0.44f, 0.72f, 1.0f));
	}
	if (IsCurveAsset(Item.Path))
	{
		return ImGui::GetColorU32(ImVec4(0.42f, 0.50f, 0.78f, 1.0f));
	}
	if (Item.Extension == ".prefab")
	{
		return ImGui::GetColorU32(ImVec4(0.58f, 0.72f, 0.92f, 1.0f));
	}
	if (Item.Extension == ".lua")
	{
		return ImGui::GetColorU32(ImVec4(0.52f, 0.72f, 0.58f, 1.0f));
	}
	if (Item.Extension == ".rml" || Item.Extension == ".rcss")
	{
		return ImGui::GetColorU32(ImVec4(0.72f, 0.60f, 0.38f, 1.0f));
	}
	if (Item.Extension == ".png")
	{
		return ImGui::GetColorU32(ImVec4(0.70f, 0.52f, 0.38f, 1.0f));
	}
	if (Item.Extension == ".hlsl" || Item.Extension == ".hlsli" || Item.Extension == ".fx")
	{
		return ImGui::GetColorU32(ImVec4(0.38f, 0.58f, 0.86f, 1.0f));
	}
	return ImGui::GetColorU32(ImVec4(0.35f, 0.38f, 0.44f, 1.0f));
}

bool FEditorContentBrowserWidget::IsPathAllowed(const std::filesystem::path& Path) const
{
	const std::filesystem::path Normalized = Path.lexically_normal();
	for (const std::filesystem::path& BrowserRoot : BrowserRootPaths)
	{
		const std::filesystem::path Relative = Normalized.lexically_relative(BrowserRoot);
		if (Normalized == BrowserRoot || (!Relative.empty() && !IsParentDirectoryReference(Relative)))
		{
			return true;
		}
	}
	return false;
}

bool FEditorContentBrowserWidget::IsProjectRootPath(const std::filesystem::path& Path) const
{
	return Path.lexically_normal() == RootPath.lexically_normal();
}

bool FEditorContentBrowserWidget::IsPreviewableImage(const FString& Extension) const
{
	return Extension == ".png" || Extension == ".jpg" || Extension == ".jpeg" || Extension == ".dds";
}

bool FEditorContentBrowserWidget::IsMaterialAsset(const FString& Extension) const
{
	return Extension == ".mat" || Extension == ".matinst";
}

bool FEditorContentBrowserWidget::IsCurveAsset(const std::filesystem::path& Path) const
{
	const FString Filename = ToLower(FPaths::ToUtf8(Path.filename().wstring()));
	const FString Extension = ToLower(FPaths::ToUtf8(Path.extension().wstring()));
	if (Extension == ".curve")
	{
		return true;
	}

	const FString CurveJsonSuffix = ".curve.json";
	if (Filename.size() < CurveJsonSuffix.size())
	{
		return false;
	}
	return Filename.compare(Filename.size() - CurveJsonSuffix.size(), CurveJsonSuffix.size(), CurveJsonSuffix) == 0;
}

bool FEditorContentBrowserWidget::IsPrefabAsset(const FString& Extension) const
{
	return Extension == ".prefab";
}

std::filesystem::path FEditorContentBrowserWidget::ResolveLuaScriptCreateDirectory() const
{
	const std::filesystem::path AssetScriptDir = (RootPath / L"Asset" / L"Script").lexically_normal();
	const std::filesystem::path LuaScriptDir = (RootPath / L"LuaScript").lexically_normal();
	const std::filesystem::path NormalizedCurrent = CurrentPath.lexically_normal();

	auto IsInsideDir = [](const std::filesystem::path& Path, const std::filesystem::path& Dir)
	{
		const std::filesystem::path Relative = Path.lexically_relative(Dir);
		return Path == Dir || (!Relative.empty() && !IsParentDirectoryReference(Relative));
	};

	if (IsInsideDir(NormalizedCurrent, AssetScriptDir) || IsInsideDir(NormalizedCurrent, LuaScriptDir))
	{
		return NormalizedCurrent;
	}
	return AssetScriptDir;
}

FString FEditorContentBrowserWidget::MakeRelativeProjectPath(const std::filesystem::path& Path) const
{
	const std::filesystem::path Relative = Path.lexically_normal().lexically_relative(RootPath);
	if (!Relative.empty() && !IsParentDirectoryReference(Relative))
	{
		return FPaths::Normalize(FPaths::ToUtf8(Relative.generic_wstring()));
	}
	return FPaths::Normalize(FPaths::ToUtf8(Path.wstring()));
}
