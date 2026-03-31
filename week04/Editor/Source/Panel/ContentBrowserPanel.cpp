#include "ContentBrowserPanel.h"

#include "Content/ContentBrowserDragDrop.h"
#include "Content/EditorContentIndex.h"
#include "Editor/EditorContext.h"
#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace
{
    constexpr float DefaultFolderPaneWidth = 250.0f;
    constexpr float MinFolderPaneWidth = 140.0f;
    constexpr float MinItemsPaneWidth = 220.0f;
    constexpr float SplitterWidth = 6.0f;
    constexpr float FolderTreeIndentSpacing = 4.0f;
    constexpr const char* TypeFilterLabels[] = {
        "All", "Scene", "Texture", "Font", "Sprite Atlas", "StaticMesh",
        "Material", "Unknown"
    };

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

    FString PathToUtf8String(const std::filesystem::path& Path)
    {
        const std::u8string Utf8Path = Path.u8string();
        return FString(reinterpret_cast<const char*>(Utf8Path.data()), Utf8Path.size());
    }

    FString BuildRelativeDisplayPath(const FString& RootVirtualPath, const FString& ItemVirtualPath)
    {
        if (ItemVirtualPath == RootVirtualPath)
        {
            return ItemVirtualPath;
        }

        if (ItemVirtualPath.size() <= RootVirtualPath.size())
        {
            return ItemVirtualPath;
        }

        const size_t PrefixLength = RootVirtualPath.size();
        if (ItemVirtualPath.compare(0, PrefixLength, RootVirtualPath) != 0)
        {
            return ItemVirtualPath;
        }

        FString RelativePath = ItemVirtualPath.substr(PrefixLength);
        if (!RelativePath.empty() && RelativePath.front() == '/')
        {
            RelativePath.erase(RelativePath.begin());
        }

        return RelativePath.empty() ? ItemVirtualPath : RelativePath;
    }

    const char* GetItemTypeLabel(EContentBrowserItemType ItemType)
    {
        switch (ItemType)
        {
        case EContentBrowserItemType::Scene:
            return "Scene";
        case EContentBrowserItemType::Texture:
            return "Texture";
        case EContentBrowserItemType::Folder:
            return "Folder";
        case EContentBrowserItemType::Font:
            return "Font";
        case EContentBrowserItemType::SpriteAtlas:
            return "Sprite Atlas";
        case EContentBrowserItemType::StaticMesh:
            return "Static Mesh";
        case EContentBrowserItemType::Material:
            return "Material";
        case EContentBrowserItemType::UnknownFile:
        default:
            return "Unknown";
        }
    }

    ImVec4 GetItemTypeColor(EContentBrowserItemType ItemType)
    {
        switch (ItemType)
        {
        case EContentBrowserItemType::Scene:
            return ImVec4(0.47f, 0.72f, 0.96f, 1.0f);
        case EContentBrowserItemType::Texture:
            return ImVec4(0.53f, 0.82f, 0.47f, 1.0f);
        case EContentBrowserItemType::Folder:
            return ImVec4(0.92f, 0.78f, 0.42f, 1.0f);
        case EContentBrowserItemType::Font:
            return ImVec4(1.f, 1.f, 1.f, 1.f);
        case EContentBrowserItemType::SpriteAtlas:
            return ImVec4(0.98f, 0.58f, 0.29f, 1.0f);
        case EContentBrowserItemType::StaticMesh:
            return ImVec4(0.40f, 0.85f, 0.90f, 1.0f); // 청록색(Cyan) 계열
        case EContentBrowserItemType::Material:
            return ImVec4(1.00f, 0.00f, 0.00f, 1.0f); // 올리브/녹색 계열
        case EContentBrowserItemType::UnknownFile:
        default:
            return ImVec4(0.76f, 0.76f, 0.76f, 1.0f);
        }
    }

    template <size_t BufferSize>
    void CopyUtf8StringToBuffer(const FString& Source, char (&Destination)[BufferSize])
    {
        static_assert(BufferSize > 0);
        const size_t CopyLength = std::min(Source.size(), BufferSize - 1);
        if (CopyLength > 0)
        {
            memcpy(Destination, Source.data(), CopyLength);
        }
        Destination[CopyLength] = '\0';
    }

    Engine::Component::EComponentAssetPathKind GetAssetPathKind(
        const FContentBrowserItem& Item)
    {
        const FString Extension = ToLowerAsciiCopy(Item.Extension);
        if (Extension == ".font")
        {
            return Engine::Component::EComponentAssetPathKind::FontFile;
        }

        if (Item.ItemType == EContentBrowserItemType::SpriteAtlas)
        {
            return Engine::Component::EComponentAssetPathKind::SpriteAtlasFile;
        }

        if (Item.ItemType == EContentBrowserItemType::Texture)
        {
            return Engine::Component::EComponentAssetPathKind::TextureImage;
        }

        if (Item.ItemType == EContentBrowserItemType::Scene)
        {
            return Engine::Component::EComponentAssetPathKind::SceneFile;
        }
        if (Item.ItemType == EContentBrowserItemType::StaticMesh)
        {
            return Engine::Component::EComponentAssetPathKind::StaticMeshFile; // (열거형에 없다면
                                                                               // 추가 필요)
        }
        if (Item.ItemType == EContentBrowserItemType::Material)
        {
            return Engine::Component::EComponentAssetPathKind::MaterialFile; // (열거형에 없다면
                                                                             // 추가 필요)
        }

        return Engine::Component::EComponentAssetPathKind::Any;
    }
} // namespace

FContentBrowserPanel::FContentBrowserPanel()
{
    SearchBuffer.fill('\0');
    LeftPaneWidth = DefaultFolderPaneWidth;
}

const wchar_t* FContentBrowserPanel::GetPanelID() const
{
    return L"ContentBrowserPanel";
}

const wchar_t* FContentBrowserPanel::GetDisplayName() const
{
    return L"Content Browser";
}

void FContentBrowserPanel::OnOpen()
{
    if (GetContext() == nullptr || GetContext()->ContentIndex == nullptr)
    {
        return;
    }

    SyncPaneWidthFromContext();
    GetContext()->ContentIndex->Refresh();
    EnsureCurrentFolderIsValid(*GetContext()->ContentIndex);
}

void FContentBrowserPanel::Draw()
{
    if (!ImGui::Begin("Content Browser", nullptr))
    {
        ImGui::End();
        return;
    }

    if (GetContext() == nullptr || GetContext()->ContentIndex == nullptr)
    {
        DrawUnavailableState();
        ImGui::End();
        return;
    }

    FEditorContentIndex& ContentIndex = *GetContext()->ContentIndex;
    SyncPaneWidthFromContext();
    EnsureCurrentFolderIsValid(ContentIndex);

    DrawToolbar(ContentIndex);
    ImGui::Separator();

    const FContentIndexSnapshot& Snapshot = ContentIndex.GetSnapshot();
    if (!Snapshot.bHasContentRoot)
    {
        ImGui::TextUnformatted("Content directory is not available.");
        ImGui::Spacing();
        const FString ContentRootText = PathToUtf8String(Snapshot.ContentRootPath);
        ImGui::TextWrapped("Expected root: %s", ContentRootText.c_str());
        ImGui::End();
        return;
    }

    DrawResizableLayout(ContentIndex, Snapshot.RootFolder);

    ImGui::End();
}

void FContentBrowserPanel::DrawUnavailableState() const
{
    ImGui::TextUnformatted("Content index is unavailable.");
    ImGui::Spacing();
    ImGui::TextWrapped("The content browser requires an initialized editor content index.");
}

void FContentBrowserPanel::DrawToolbar(FEditorContentIndex& ContentIndex)
{
    if (ImGui::Button("Refresh"))
    {
        ContentIndex.Refresh();
        EnsureCurrentFolderIsValid(ContentIndex);
    }

    ImGui::SameLine();
    const FContentIndexSnapshot& Snapshot = ContentIndex.GetSnapshot();
    ImGui::Text("Folders: %d  Files: %d", Snapshot.FolderCount, Snapshot.FileCount);
}

void FContentBrowserPanel::DrawResizableLayout(FEditorContentIndex& ContentIndex,
                                               const FContentBrowserFolderNode& RootFolder)
{
    const ImVec2 AvailableRegion = ImGui::GetContentRegionAvail();
    if (AvailableRegion.x <= 0.0f || AvailableRegion.y <= 0.0f)
    {
        return;
    }

    float LeftWidth = LeftPaneWidth;
    if (AvailableRegion.x > (MinFolderPaneWidth + MinItemsPaneWidth + SplitterWidth))
    {
        const float MaxLeftWidth = AvailableRegion.x - MinItemsPaneWidth - SplitterWidth;
        LeftWidth = FMath::Clamp(LeftWidth, MinFolderPaneWidth, MaxLeftWidth);
    }
    else
    {
        LeftWidth = std::max((AvailableRegion.x - SplitterWidth) * 0.5f, 1.0f);
    }

    LeftPaneWidth = LeftWidth;
    if (GetContext() != nullptr)
    {
        GetContext()->ContentBrowserLeftPaneWidth = LeftPaneWidth;
    }

    if (ImGui::BeginChild("##ContentFolders", ImVec2(LeftWidth, 0.0f),
                          ImGuiChildFlags_Borders))
    {
        DrawFolderTree(RootFolder);
    }
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 0.0f);

    const ImVec2 SplitterStart = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##ContentBrowserSplitter", ImVec2(SplitterWidth, AvailableRegion.y));
    const bool bSplitterHovered = ImGui::IsItemHovered();
    const bool bSplitterActive = ImGui::IsItemActive();
    if (bSplitterHovered || bSplitterActive)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }

    if (bSplitterActive)
    {
        LeftPaneWidth += ImGui::GetIO().MouseDelta.x;
        if (AvailableRegion.x > (MinFolderPaneWidth + MinItemsPaneWidth + SplitterWidth))
        {
            const float MaxLeftWidth = AvailableRegion.x - MinItemsPaneWidth - SplitterWidth;
            LeftPaneWidth = FMath::Clamp(LeftPaneWidth, MinFolderPaneWidth, MaxLeftWidth);
        }
        else
        {
            LeftPaneWidth = std::max((AvailableRegion.x - SplitterWidth) * 0.5f, 1.0f);
        }

        if (GetContext() != nullptr)
        {
            GetContext()->ContentBrowserLeftPaneWidth = LeftPaneWidth;
        }
    }

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    const ImVec2 SplitterEnd =
        ImVec2(SplitterStart.x + SplitterWidth, SplitterStart.y + AvailableRegion.y);
    const ImU32 SplitterColor =
        ImGui::GetColorU32(bSplitterActive ? ImVec4(0.37f, 0.58f, 0.96f, 1.0f)
                                           : bSplitterHovered
                                               ? ImVec4(0.30f, 0.30f, 0.32f, 1.0f)
                                               : ImVec4(0.22f, 0.22f, 0.24f, 1.0f));
    DrawList->AddRectFilled(SplitterStart, SplitterEnd, SplitterColor);

    ImGui::SameLine(0.0f, 0.0f);

    if (ImGui::BeginChild("##ContentItems", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
    {
        DrawCurrentFolderView(ContentIndex);
    }
    ImGui::EndChild();
}

void FContentBrowserPanel::DrawFolderTree(const FContentBrowserFolderNode& RootFolder)
{
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, FolderTreeIndentSpacing);
    DrawFolderTreeNode(RootFolder, true);
    ImGui::PopStyleVar();
}

void FContentBrowserPanel::DrawFolderTreeNode(const FContentBrowserFolderNode& Folder, bool bIsRoot)
{
    const bool bIsSelected = (CurrentFolderVirtualPath == Folder.VirtualPath);
    ImGuiTreeNodeFlags TreeFlags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

    if (Folder.ChildFolders.empty())
    {
        TreeFlags |= ImGuiTreeNodeFlags_Leaf;
    }

    if (bIsRoot)
    {
        TreeFlags |= ImGuiTreeNodeFlags_DefaultOpen;
    }

    if (bIsSelected)
    {
        TreeFlags |= ImGuiTreeNodeFlags_Selected;
    }

    const bool bOpened =
        ImGui::TreeNodeEx(Folder.VirtualPath.c_str(), TreeFlags, "%s", Folder.DisplayName.c_str());

    if (ImGui::IsItemClicked())
    {
        CurrentFolderVirtualPath = Folder.VirtualPath;
        SelectedItemVirtualPath = Folder.VirtualPath;
    }

    if (!bOpened)
    {
        return;
    }

    for (const FContentBrowserFolderNode& ChildFolder : Folder.ChildFolders)
    {
        DrawFolderTreeNode(ChildFolder, false);
    }

    ImGui::TreePop();
}

void FContentBrowserPanel::DrawCurrentFolderView(FEditorContentIndex& ContentIndex)
{
    const FContentBrowserFolderNode* CurrentFolder =
        ContentIndex.FindFolderByVirtualPath(CurrentFolderVirtualPath);
    if (CurrentFolder == nullptr)
    {
        ImGui::TextUnformatted("Current folder could not be resolved.");
        return;
    }

    ImGui::Text("%s", CurrentFolder->VirtualPath.c_str());
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputText("Search", SearchBuffer.data(), SearchBuffer.size());

    ImGui::SameLine();
    int32 FilterIndex = static_cast<int32>(TypeFilter);
    ImGui::SetNextItemWidth(140.0f);
    if (ImGui::Combo("Type", &FilterIndex, TypeFilterLabels, IM_ARRAYSIZE(TypeFilterLabels)))
    {
        TypeFilter = static_cast<EItemTypeFilter>(FilterIndex);
    }

    ImGui::Separator();

    if (!ImGui::BeginTable("##ContentBrowserTable", 2,
                           ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                               ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable))
    {
        return;
    }

    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 110.0f);
    ImGui::TableHeadersRow();

    int32 VisibleItemCount = 0;
    const bool bHasActiveSearch = !FString(SearchBuffer.data()).empty();

    TArray<const FContentBrowserFolderNode*> VisibleFolders;
    TArray<const FContentBrowserItem*> VisibleFiles;
    if (bHasActiveSearch)
    {
        CollectVisibleFolders(*CurrentFolder, *CurrentFolder, VisibleFolders);
        CollectVisibleFiles(*CurrentFolder, VisibleFiles);
    }
    else
    {
        VisibleFolders.reserve(CurrentFolder->ChildFolders.size());
        for (const FContentBrowserFolderNode& ChildFolder : CurrentFolder->ChildFolders)
        {
            VisibleFolders.push_back(&ChildFolder);
        }

        VisibleFiles.reserve(CurrentFolder->Files.size());
        for (const FContentBrowserItem& FileItem : CurrentFolder->Files)
        {
            VisibleFiles.push_back(&FileItem);
        }
    }

    for (const FContentBrowserFolderNode* ChildFolder : VisibleFolders)
    {
        if (ChildFolder == nullptr)
        {
            continue;
        }

        ++VisibleItemCount;
        ImGui::PushID(ChildFolder->VirtualPath.c_str());
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        const FString DisplayLabel =
            BuildRelativeDisplayPath(CurrentFolder->VirtualPath, ChildFolder->VirtualPath);
        const bool bIsSelected = (SelectedItemVirtualPath == ChildFolder->VirtualPath);
        if (ImGui::Selectable(DisplayLabel.c_str(), bIsSelected,
                              ImGuiSelectableFlags_SpanAllColumns |
                                  ImGuiSelectableFlags_AllowDoubleClick))
        {
            SelectedItemVirtualPath = ChildFolder->VirtualPath;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                CurrentFolderVirtualPath = ChildFolder->VirtualPath;
            }
        }

        ImGui::TableSetColumnIndex(1);
        ImGui::TextColored(GetItemTypeColor(EContentBrowserItemType::Folder), "%s",
                           GetItemTypeLabel(EContentBrowserItemType::Folder));
        ImGui::PopID();
    }

    for (const FContentBrowserItem* FileItem : VisibleFiles)
    {
        if (FileItem == nullptr || !PassesTypeFilter(FileItem->ItemType))
        {
            continue;
        }

        ++VisibleItemCount;
        ImGui::PushID(FileItem->VirtualPath.c_str());
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        const FString DisplayLabel =
            BuildRelativeDisplayPath(CurrentFolder->VirtualPath, FileItem->VirtualPath);
        const bool bIsSelected = (SelectedItemVirtualPath == FileItem->VirtualPath);
        if (ImGui::Selectable(DisplayLabel.c_str(), bIsSelected,
                              ImGuiSelectableFlags_SpanAllColumns |
                                  ImGuiSelectableFlags_AllowDoubleClick))
        {
            SelectedItemVirtualPath = FileItem->VirtualPath;
        }

        if (ImGui::BeginDragDropSource())
        {
            Editor::Content::FContentBrowserAssetDragDropPayload Payload;
            Payload.ItemType = FileItem->ItemType;
            Payload.AssetKind = GetAssetPathKind(*FileItem);
            CopyUtf8StringToBuffer(FileItem->VirtualPath, Payload.VirtualPath);
            CopyUtf8StringToBuffer(PathToUtf8String(FileItem->AbsolutePath), Payload.AbsolutePath);

            ImGui::SetDragDropPayload(Editor::Content::ContentBrowserAssetPayloadType, &Payload,
                                      sizeof(Payload));
            ImGui::TextUnformatted(FileItem->DisplayName.c_str());
            ImGui::TextDisabled("%s", FileItem->VirtualPath.c_str());
            ImGui::EndDragDropSource();
        }

        ImGui::TableSetColumnIndex(1);
        ImGui::TextColored(GetItemTypeColor(FileItem->ItemType), "%s",
                           GetItemTypeLabel(FileItem->ItemType));
        ImGui::PopID();
    }

    if (VisibleItemCount == 0)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("No items matched the current filter.");
    }

    ImGui::EndTable();
}

void FContentBrowserPanel::EnsureCurrentFolderIsValid(const FEditorContentIndex& ContentIndex)
{
    if (ContentIndex.FindFolderByVirtualPath(CurrentFolderVirtualPath) != nullptr)
    {
        return;
    }

    CurrentFolderVirtualPath = "/Game";
    SelectedItemVirtualPath.clear();
}

void FContentBrowserPanel::CollectVisibleFolders(
    const FContentBrowserFolderNode& ParentFolder, const FContentBrowserFolderNode& Folder,
    TArray<const FContentBrowserFolderNode*>& OutFolders) const
{
    for (const FContentBrowserFolderNode& ChildFolder : Folder.ChildFolders)
    {
        if (PassesSearchFilter(BuildRelativeDisplayPath(ParentFolder.VirtualPath,
                                                        ChildFolder.VirtualPath)))
        {
            OutFolders.push_back(&ChildFolder);
        }

        CollectVisibleFolders(ParentFolder, ChildFolder, OutFolders);
    }
}

void FContentBrowserPanel::CollectVisibleFiles(
    const FContentBrowserFolderNode& Folder, TArray<const FContentBrowserItem*>& OutFiles) const
{
    for (const FContentBrowserItem& FileItem : Folder.Files)
    {
        if (PassesSearchFilter(BuildRelativeDisplayPath(CurrentFolderVirtualPath,
                                                        FileItem.VirtualPath)))
        {
            OutFiles.push_back(&FileItem);
        }
    }

    for (const FContentBrowserFolderNode& ChildFolder : Folder.ChildFolders)
    {
        CollectVisibleFiles(ChildFolder, OutFiles);
    }
}

void FContentBrowserPanel::SyncPaneWidthFromContext()
{
    if (GetContext() == nullptr)
    {
        return;
    }

    if (!bHasInitializedPaneWidth)
    {
        LeftPaneWidth = std::max(GetContext()->ContentBrowserLeftPaneWidth, MinFolderPaneWidth);
        bHasInitializedPaneWidth = true;
        return;
    }

    GetContext()->ContentBrowserLeftPaneWidth = LeftPaneWidth;
}

bool FContentBrowserPanel::PassesSearchFilter(const FString& DisplayName) const
{
    const FString Query = ToLowerAsciiCopy(FString(SearchBuffer.data()));
    if (Query.empty())
    {
        return true;
    }

    return ToLowerAsciiCopy(DisplayName).find(Query) != FString::npos;
}

bool FContentBrowserPanel::PassesTypeFilter(EContentBrowserItemType ItemType) const
{
    switch (TypeFilter)
    {
    case EItemTypeFilter::All:
        return true;
    case EItemTypeFilter::Scene:
        return ItemType == EContentBrowserItemType::Scene;
    case EItemTypeFilter::Texture:
        return ItemType == EContentBrowserItemType::Texture;
    case EItemTypeFilter::Font:
        return ItemType == EContentBrowserItemType::Font;
    case EItemTypeFilter::SpriteAtlas:
        return ItemType == EContentBrowserItemType::SpriteAtlas;
    case EItemTypeFilter::StaticMesh: // <-- 추가
        return ItemType == EContentBrowserItemType::StaticMesh;
    case EItemTypeFilter::Material: // <-- 추가
        return ItemType == EContentBrowserItemType::Material;
    case EItemTypeFilter::UnknownFile:
        return ItemType == EContentBrowserItemType::UnknownFile;
    default:
        return true;
    }
}
