#pragma once

#include "Panel.h"

#include <array>

class FEditorContentIndex;
struct FContentBrowserFolderNode;
struct FContentBrowserItem;
enum class EContentBrowserItemType : uint8;

class FContentBrowserPanel : public IPanel
{
  public:
    FContentBrowserPanel();

    const wchar_t* GetPanelID() const override;
    const wchar_t* GetDisplayName() const override;
    int32 GetWindowMenuOrder() const override { return 40; }

    void OnOpen() override;
    void Draw() override;

  private:
    enum class EItemTypeFilter : int32
    {
        All = 0,
        Scene,
        Texture,
        Font,
        SpriteAtlas,
        UnknownFile
    };

  private:
    void DrawUnavailableState() const;
    void DrawToolbar(FEditorContentIndex& ContentIndex);
    void DrawResizableLayout(FEditorContentIndex& ContentIndex,
                             const FContentBrowserFolderNode& RootFolder);
    void DrawFolderTree(const FContentBrowserFolderNode& RootFolder);
    void DrawFolderTreeNode(const FContentBrowserFolderNode& Folder, bool bIsRoot);
    void DrawCurrentFolderView(FEditorContentIndex& ContentIndex);
    void CollectVisibleFolders(const FContentBrowserFolderNode& ParentFolder,
                               const FContentBrowserFolderNode& Folder,
                               TArray<const FContentBrowserFolderNode*>& OutFolders) const;
    void CollectVisibleFiles(const FContentBrowserFolderNode& Folder,
                             TArray<const FContentBrowserItem*>& OutFiles) const;
    void EnsureCurrentFolderIsValid(const FEditorContentIndex& ContentIndex);
    void SyncPaneWidthFromContext();

    bool PassesSearchFilter(const FString& DisplayName) const;
    bool PassesTypeFilter(EContentBrowserItemType ItemType) const;

  private:
    std::array<char, 256> SearchBuffer{};
    FString CurrentFolderVirtualPath = "/Game";
    FString SelectedItemVirtualPath;
    EItemTypeFilter TypeFilter = EItemTypeFilter::All;
    float LeftPaneWidth = 250.0f;
    bool bHasInitializedPaneWidth = false;
};
