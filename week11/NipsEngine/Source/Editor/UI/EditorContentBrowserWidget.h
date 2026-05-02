#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Render/Common/ComPtr.h"
#include "ImGui/imgui.h"

#include <filesystem>

class UMaterialInterface;
class UStaticMesh;
struct ID3D11ShaderResourceView;
struct ID3D11Texture2D;

class FEditorContentBrowserWidget : public FEditorWidget
{
public:
	enum class EPresentationMode
	{
		Drawer,
		FloatingWindow,
	};

	void Initialize(UEditorEngine* InEditorEngine) override;
	void Render(float DeltaTime) override;
	void Refresh();
	void SetVisible(bool bInVisible) { bVisible = bInVisible; }
	bool IsVisible() const { return bVisible; }
	void ToggleVisible() { bVisible = !bVisible; }
	void SetPresentationMode(EPresentationMode InMode) { PresentationMode = InMode; }
	EPresentationMode GetPresentationMode() const { return PresentationMode; }
	bool IsDrawerMode() const { return PresentationMode == EPresentationMode::Drawer; }
	bool IsFloatingWindowMode() const { return PresentationMode == EPresentationMode::FloatingWindow; }
	void OpenAssetRoot();
	bool IsMouseOverBrowser() const;
	bool ConsumeReleasedDragPayload(FString& OutPayloadType, FString& OutPayloadPath);

private:
	struct FContentItem
	{
		std::filesystem::path Path;
		FString Name;
		FString Extension;
		bool bIsDirectory = false;
	};

	struct FDirNode
	{
		std::filesystem::path Path;
		FString Name;
		TArray<FDirNode> Children;
	};

	struct FMaterialPreviewSnapshot
	{
		TComPtr<ID3D11Texture2D> Texture;
		TComPtr<ID3D11ShaderResourceView> SRV;
		uint32 Width = 0;
		uint32 Height = 0;
	};

	void LoadFromSettings();
	void SaveToSettings() const;
	void RefreshContent();
	void RebuildRootNode();
	FDirNode BuildDirectoryTree(const std::filesystem::path& DirPath) const;
	TArray<FContentItem> ReadDirectory(const std::filesystem::path& DirPath) const;
	void DrawBrowserContents();
	void DrawToolbar();
	void DrawDirectoryNode(const FDirNode& Node);
	void DrawContentGrid();
	void DrawContentTile(const FContentItem& Item, const ImVec2& TileSize);
	void DrawContentContextMenu(bool bHasSelectedItem);
	bool CreateFolder();
	bool CreateTextFile();
	bool CreateMaterialAsset();
	bool DeleteSelectedItem();
	void RequestRenameSelectedItem();
	bool CommitRename();
	void DrawRenamePopup();
	std::filesystem::path MakeUniquePath(const std::filesystem::path& DesiredPath) const;
	void DrawFolderIcon(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color) const;
	void DrawDetails();
	void DrawAssetPreview();
	void NavigateTo(const std::filesystem::path& Path);
	void NavigateTo(const std::filesystem::path& Path, bool bAddHistory);
	void NavigateBack();
	FString MakeDisplayPath(const std::filesystem::path& Path) const;
	FString GetPayloadType(const FContentItem& Item) const;
	ImU32 GetItemColor(const FContentItem& Item) const;
	ID3D11ShaderResourceView* GetImagePreviewSRV(const FContentItem& Item);
	ID3D11ShaderResourceView* GetMaterialPreviewSRV(const FContentItem& Item, uint32 Width, uint32 Height, bool bHighPriority = false);
	bool CapturePreviewSnapshot(ID3D11ShaderResourceView* SourceSRV, FMaterialPreviewSnapshot& OutSnapshot, uint32 Width, uint32 Height);
	UMaterialInterface* ResolveMaterialAsset(const std::filesystem::path& Path);
	bool IsPathAllowed(const std::filesystem::path& Path) const;
	bool IsProjectRootPath(const std::filesystem::path& Path) const;
	bool IsPreviewableImage(const FString& Extension) const;
	bool IsMaterialAsset(const FString& Extension) const;
	bool IsPrefabAsset(const FString& Extension) const;
	FString MakeRelativeProjectPath(const std::filesystem::path& Path) const;

private:
	FDirNode RootNode;
	TArray<FContentItem> CurrentItems;
	TArray<std::filesystem::path> BrowserRootPaths;
	std::filesystem::path RootPath;
	std::filesystem::path CurrentPath;
	std::filesystem::path PendingRevealPath;
	std::filesystem::path SelectedPath;
	TArray<std::filesystem::path> BackHistory;
	TMap<FString, FMaterialPreviewSnapshot> MaterialPreviewCache;
	FString SearchFilter;
	char RenameBuffer[260] = {};
	float TileSize = 72.0f;
	float AnimAlpha = 0.0f;
	int32 MaterialPreviewBuildsThisFrame = 0;
	bool bVisible = false;
	bool bNeedsRefresh = true;
	bool bPendingMaterialPreviewCacheClear = false;
	bool bRenamePopupRequested = false;
	bool bMouseOverBrowser = false;
	bool bHasBrowserScreenRect = false;
	bool bOpenContentContextMenu = false;
	bool bContentContextMenuHasSelection = false;
	ImVec2 BrowserScreenMin = ImVec2(0.0f, 0.0f);
	ImVec2 BrowserScreenMax = ImVec2(0.0f, 0.0f);
	EPresentationMode PresentationMode = EPresentationMode::Drawer;
	UStaticMesh* MaterialPreviewMesh = nullptr;
	std::filesystem::path RenameSourcePath;
	FString ActiveDragPayloadType;
	FString ActiveDragPayloadPath;
};
