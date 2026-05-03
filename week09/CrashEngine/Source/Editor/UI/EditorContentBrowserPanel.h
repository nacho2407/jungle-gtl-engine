// 에디터 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"
#include "Editor/UI/EditorPanel.h"

#include <filesystem>
#include <map>

struct ImVec2;

enum class EContentBrowserAssetType : int32
{
    Unknown,
    Folder,
    Scene,
    LuaScript,
    Material,
    Model,
    Texture,
    Prefab,
};

struct FContentBrowserDragPayload
{
    EContentBrowserAssetType Type = EContentBrowserAssetType::Unknown;
    char Path[260] = {};
};

class FEditorContentBrowserPanel : public FEditorPanel
{
public:
    virtual void Initialize(UEditorEngine* InEditorEngine) override;
    virtual void Render(float DeltaTime) override;

private:
    struct FContentItem
    {
        FString Name;
        FString Extension;
        FString RelativePath;
        std::filesystem::path FullPath;
        EContentBrowserAssetType Type = EContentBrowserAssetType::Unknown;
        bool bDirectory = false;
    };

    void Refresh();
    void NavigateTo(const std::filesystem::path& Directory);
    void NavigateUp();

    void RenderToolbar();
    void RenderPathBar();
    void RenderItems();
    void RenderItem(const FContentItem& Item);
    void DrawItemTile(const FContentItem& Item, const ImVec2& TileSize);
    void DrawTileThumbnail(const FContentItem& Item, const ImVec2& Min, const ImVec2& Max);
    void DrawFallbackIcon(EContentBrowserAssetType Type, const ImVec2& Min, const ImVec2& Max);
    void RenderBackgroundContextMenu();
    void RenderItemContextMenu(const FContentItem& Item);
    void RenderCreateFolderPopup();
    void RenderCreatePrefabPopup();

    bool IsInsideContentRoot(const std::filesystem::path& Path) const;
    FString MakeDisplayPath(const std::filesystem::path& Path) const;
    FString MakeContentRelativePath(const std::filesystem::path& Path) const;
    EContentBrowserAssetType GetAssetType(const std::filesystem::directory_entry& Entry) const;
    const char* GetTypeLabel(EContentBrowserAssetType Type) const;
    bool ShouldPreviewTexture(const FContentItem& Item) const;
    class UTexture2D* GetThumbnailTexture(const FString& Path);
    class UTexture2D* GetEditorIconTexture(const FString& FileName);

    bool CreateFolder(const FString& FolderName);
    bool CreateEmptyPrefab(const FString& FileName);

private:
    std::filesystem::path ContentRoot;
    std::filesystem::path CurrentDirectory;
    std::filesystem::path SelectedPath;
    FString CurrentDisplayPath = "Content";
    FString SelectedRelativePath;
    TArray<FContentItem> Items;
    std::map<FString, UTexture2D*> ThumbnailCache;
    std::map<FString, UTexture2D*> EditorIconCache;

    bool bNeedsRefresh = true;
    char NewFolderName[128] = "NewFolder";
    char NewPrefabName[128] = "NewPrefab";
};
