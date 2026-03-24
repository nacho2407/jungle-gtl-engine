#include "EditorContentIndex.h"

#include "Core/Path.h"
#include "Core/Logging/LogMacros.h"

#include <algorithm>
#include <cwctype>

namespace
{
    constexpr const char* GameVirtualRoot = "/Game";

    FString PathToUtf8String(const std::filesystem::path& Path)
    {
        const std::u8string Utf8Path = Path.u8string();
        return FString(reinterpret_cast<const char*>(Utf8Path.data()), Utf8Path.size());
    }

    FString ToLowerAsciiCopy(const FString& Value)
    {
        FString LowerValue = Value;
        std::transform(
            LowerValue.begin(), LowerValue.end(), LowerValue.begin(),
            [](char Character)
            {
                return static_cast<char>(std::tolower(static_cast<unsigned char>(Character)));
            });
        return LowerValue;
    }

    FWString ToLowerWideCopy(const FWString& Value)
    {
        FWString LowerValue = Value;
        std::transform(
            LowerValue.begin(), LowerValue.end(), LowerValue.begin(),
            [](wchar_t Character)
            {
                return static_cast<wchar_t>(std::towlower(Character));
            });
        return LowerValue;
    }

    EContentBrowserItemType ClassifyFileType(const std::filesystem::path& FilePath)
    {
        FWString Extension = ToLowerWideCopy(FilePath.extension().wstring());
        if (Extension == L".scene")
        {
            return EContentBrowserItemType::Scene;
        }

        if (Extension == L".png" || Extension == L".jpg" || Extension == L".jpeg" ||
            Extension == L".bmp")
        {
            return EContentBrowserItemType::Texture;
        }

        if (Extension == L".font" || Extension == L".Font")
        {
            return EContentBrowserItemType::Font;
        }

        return EContentBrowserItemType::UnknownFile;
    }

    FString BuildVirtualPath(const std::filesystem::path& ContentRoot,
                             const std::filesystem::path& TargetPath)
    {
        std::error_code ErrorCode;
        std::filesystem::path RelativePath = std::filesystem::relative(TargetPath, ContentRoot, ErrorCode);
        if (ErrorCode)
        {
            ErrorCode.clear();
            RelativePath = TargetPath.lexically_relative(ContentRoot);
        }

        FString VirtualPath = GameVirtualRoot;
        if (RelativePath.empty() || RelativePath == ".")
        {
            return VirtualPath;
        }

        for (const std::filesystem::path& Part : RelativePath)
        {
            if (Part.empty() || Part == ".")
            {
                continue;
            }

            VirtualPath += "/";
            VirtualPath += PathToUtf8String(Part);
        }

        return VirtualPath;
    }

    FString GetFolderDisplayName(const std::filesystem::path& ContentRoot,
                                 const std::filesystem::path& FolderPath)
    {
        if (FolderPath == ContentRoot)
        {
            return "Content";
        }

        return PathToUtf8String(FolderPath.filename());
    }

    FContentBrowserFolderNode BuildFolderNode(const std::filesystem::path& ContentRoot,
                                              const std::filesystem::path& FolderPath,
                                              int32& InOutFolderCount,
                                              int32& InOutFileCount)
    {
        FContentBrowserFolderNode FolderNode;
        FolderNode.AbsolutePath = FolderPath;
        FolderNode.VirtualPath = BuildVirtualPath(ContentRoot, FolderPath);
        FolderNode.DisplayName = GetFolderDisplayName(ContentRoot, FolderPath);
        ++InOutFolderCount;

        std::error_code ErrorCode;
        constexpr auto DirectoryOptions = std::filesystem::directory_options::skip_permission_denied;
        std::filesystem::directory_iterator Iterator(FolderPath, DirectoryOptions, ErrorCode);
        if (ErrorCode)
        {
            UE_LOG(ContentBrowser, ELogVerbosity::Warning,
                   "Failed to enumerate folder: %s", PathToUtf8String(FolderPath).c_str());
            return FolderNode;
        }

        for (const std::filesystem::directory_entry& Entry : Iterator)
        {
            if (Entry.is_directory(ErrorCode))
            {
                ErrorCode.clear();
                FolderNode.ChildFolders.push_back(
                    BuildFolderNode(ContentRoot, Entry.path(), InOutFolderCount, InOutFileCount));
                continue;
            }

            if (!Entry.is_regular_file(ErrorCode))
            {
                ErrorCode.clear();
                continue;
            }
            ErrorCode.clear();

            FContentBrowserItem Item;
            Item.AbsolutePath = Entry.path();
            Item.VirtualPath = BuildVirtualPath(ContentRoot, Entry.path());
            Item.DisplayName = PathToUtf8String(Entry.path().filename());
            Item.Extension = PathToUtf8String(Entry.path().extension());
            Item.ItemType = ClassifyFileType(Entry.path());
            FolderNode.Files.push_back(std::move(Item));
            ++InOutFileCount;
        }

        std::ranges::sort(FolderNode.ChildFolders
                          ,
                          [](const FContentBrowserFolderNode& Left, const FContentBrowserFolderNode& Right)
                          {
	                          return ToLowerAsciiCopy(Left.DisplayName) < ToLowerAsciiCopy(Right.DisplayName);
                          });

        std::ranges::sort(FolderNode.Files
                          ,
                          [](const FContentBrowserItem& Left, const FContentBrowserItem& Right)
                          {
	                          return ToLowerAsciiCopy(Left.DisplayName) < ToLowerAsciiCopy(Right.DisplayName);
                          });

        return FolderNode;
    }
} // namespace

void FEditorContentIndex::Refresh()
{
    Snapshot = {};
    Snapshot.ContentRootPath = FPaths::AppContentDir();

    std::error_code ErrorCode;
    if (!std::filesystem::exists(Snapshot.ContentRootPath, ErrorCode) ||
        !std::filesystem::is_directory(Snapshot.ContentRootPath, ErrorCode))
    {
        Snapshot.RootFolder.AbsolutePath = Snapshot.ContentRootPath;
        Snapshot.RootFolder.VirtualPath = GameVirtualRoot;
        Snapshot.RootFolder.DisplayName = "Content";
        UE_LOG(ContentBrowser, ELogVerbosity::Warning,
               "Content root was not found: %s",
               PathToUtf8String(Snapshot.ContentRootPath).c_str());
        return;
    }

    Snapshot.bHasContentRoot = true;
    Snapshot.RootFolder =
        BuildFolderNode(Snapshot.ContentRootPath, Snapshot.ContentRootPath,
                        Snapshot.FolderCount, Snapshot.FileCount);
}

const FContentBrowserFolderNode* FEditorContentIndex::FindFolderByVirtualPath(
    const FString& VirtualPath) const
{
    if (!Snapshot.bHasContentRoot)
    {
        return &Snapshot.RootFolder;
    }

    return FindFolderRecursive(Snapshot.RootFolder, VirtualPath);
}

const FContentBrowserFolderNode* FEditorContentIndex::FindFolderRecursive(
    const FContentBrowserFolderNode& Folder, const FString& VirtualPath) const
{
    if (Folder.VirtualPath == VirtualPath)
    {
        return &Folder;
    }

    for (const FContentBrowserFolderNode& ChildFolder : Folder.ChildFolders)
    {
        if (const FContentBrowserFolderNode* FoundFolder =
                FindFolderRecursive(ChildFolder, VirtualPath))
        {
            return FoundFolder;
        }
    }

    return nullptr;
}
