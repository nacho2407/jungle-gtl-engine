#pragma once

#include "Core/CoreMinimal.h"

#include <filesystem>

enum class EContentBrowserItemType : uint8
{
    Folder,
    Scene,
    Texture,
    Font,
    UnknownFile
};

struct FContentBrowserItem
{
    std::filesystem::path AbsolutePath;
    FString VirtualPath;
    FString DisplayName;
    FString Extension;
    EContentBrowserItemType ItemType = EContentBrowserItemType::UnknownFile;
};

struct FContentBrowserFolderNode
{
    std::filesystem::path AbsolutePath;
    FString VirtualPath;
    FString DisplayName;
    TArray<FContentBrowserFolderNode> ChildFolders;
    TArray<FContentBrowserItem> Files;
};

struct FContentIndexSnapshot
{
    std::filesystem::path ContentRootPath;
    bool bHasContentRoot = false;
    int32 FolderCount = 0;
    int32 FileCount = 0;
    FContentBrowserFolderNode RootFolder;
};

class FEditorContentIndex
{
  public:
    void Refresh();

    const FContentIndexSnapshot& GetSnapshot() const { return Snapshot; }
    const FContentBrowserFolderNode* FindFolderByVirtualPath(const FString& VirtualPath) const;

  private:
    const FContentBrowserFolderNode* FindFolderRecursive(
        const FContentBrowserFolderNode& Folder, const FString& VirtualPath) const;

  private:
    FContentIndexSnapshot Snapshot;
};
