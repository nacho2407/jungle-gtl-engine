// 에디터 영역의 세부 동작을 구현합니다.
#include "Editor/UI/EditorContentBrowserPanel.h"

#include "Core/Logging/LogMacros.h"
#include "Editor/EditorEngine.h"
#include "Platform/Paths.h"
#include "Texture/Texture2D.h"

#include "ImGui/imgui.h"
#include "Profiling/Stats.h"

#include <d3d11.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>

namespace
{
constexpr const char* ContentBrowserPayloadName = "CRASH_CONTENT_ASSET";

bool IsPrefabExtension(const std::filesystem::path& Path)
{
    return Path.extension() == L".prefab" || Path.extension() == L".Prefab";
}

std::filesystem::path NormalizePath(const std::filesystem::path& Path)
{
    std::error_code Ec;
    std::filesystem::path Normalized = std::filesystem::weakly_canonical(Path, Ec);
    return Ec ? Path.lexically_normal() : Normalized.lexically_normal();
}

FString ToLowerString(FString Value)
{
    std::transform(Value.begin(), Value.end(), Value.begin(), [](unsigned char Ch)
    {
        return static_cast<char>(std::tolower(Ch));
    });
    return Value;
}

void DrawCenteredText(const FString& Text, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
{
    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    const ImVec2 TextSize = ImGui::CalcTextSize(Text.c_str());
    const float CenterOffset = (Max.x - Min.x - TextSize.x) * 0.5f;
    const float X = Min.x + (CenterOffset > 0.0f ? CenterOffset : 0.0f);
    DrawList->AddText(ImVec2(X, Min.y), Color, Text.c_str());
}
} // namespace

void FEditorContentBrowserPanel::Initialize(UEditorEngine* InEditorEngine)
{
    FEditorPanel::Initialize(InEditorEngine);

    ContentRoot = NormalizePath(FPaths::ToPath(FPaths::ContentDir()));
    CurrentDirectory = ContentRoot;
    FPaths::CreateDir(FPaths::Combine(FPaths::ContentDir(), L"Prefabs"));
    Refresh();
}

void FEditorContentBrowserPanel::Render(float DeltaTime)
{
    (void)DeltaTime;

    SCOPE_STAT_CAT("ContentBrowserPanel.Render", "5_UI");

    if (bNeedsRefresh)
    {
        Refresh();
    }

    ImGui::SetNextWindowSize(ImVec2(720.0f, 360.0f), ImGuiCond_Once);
    if (!ImGui::Begin("Content Browser"))
    {
        ImGui::End();
        return;
    }

    RenderToolbar();
    RenderPathBar();
    ImGui::Separator();
    RenderItems();
    RenderBackgroundContextMenu();
    RenderCreateFolderPopup();
    RenderCreatePrefabPopup();

    ImGui::End();
}

void FEditorContentBrowserPanel::Refresh()
{
    Items.clear();
    bNeedsRefresh = false;

    if (ContentRoot.empty())
    {
        ContentRoot = NormalizePath(FPaths::ToPath(FPaths::ContentDir()));
    }

    if (CurrentDirectory.empty() || !IsInsideContentRoot(CurrentDirectory))
    {
        CurrentDirectory = ContentRoot;
    }

    CurrentDisplayPath = MakeDisplayPath(CurrentDirectory);

    std::error_code Ec;
    if (!std::filesystem::exists(CurrentDirectory, Ec))
    {
        std::filesystem::create_directories(CurrentDirectory, Ec);
    }

    if (Ec || !std::filesystem::is_directory(CurrentDirectory, Ec))
    {
        return;
    }

    for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator(CurrentDirectory, Ec))
    {
        if (Ec)
        {
            break;
        }

        FContentItem Item;
        Item.FullPath = NormalizePath(Entry.path());
        Item.Name = FPaths::FromPath(Entry.path().filename());
        Item.Extension = FPaths::FromPath(Entry.path().extension());
        Item.bDirectory = Entry.is_directory(Ec);
        Item.Type = GetAssetType(Entry);
        Item.RelativePath = MakeContentRelativePath(Item.FullPath);
        Items.push_back(Item);
    }

    std::sort(Items.begin(), Items.end(), [](const FContentItem& A, const FContentItem& B)
    {
        if (A.bDirectory != B.bDirectory)
        {
            return A.bDirectory > B.bDirectory;
        }
        return A.Name < B.Name;
    });
}

void FEditorContentBrowserPanel::NavigateTo(const std::filesystem::path& Directory)
{
    std::filesystem::path Target = NormalizePath(Directory);
    if (!IsInsideContentRoot(Target))
    {
        return;
    }

    std::error_code Ec;
    if (!std::filesystem::is_directory(Target, Ec))
    {
        return;
    }

    CurrentDirectory = Target;
    SelectedPath.clear();
    SelectedRelativePath.clear();
    CurrentDisplayPath = MakeDisplayPath(CurrentDirectory);
    bNeedsRefresh = true;
}

void FEditorContentBrowserPanel::NavigateUp()
{
    if (CurrentDirectory == ContentRoot)
    {
        return;
    }

    NavigateTo(CurrentDirectory.parent_path());
}

void FEditorContentBrowserPanel::RenderToolbar()
{
    if (ImGui::Button("Back"))
    {
        NavigateUp();
    }

    ImGui::SameLine();
    if (ImGui::Button("Refresh"))
    {
        bNeedsRefresh = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("New Folder"))
    {
        std::snprintf(NewFolderName, sizeof(NewFolderName), "NewFolder");
        ImGui::OpenPopup("Create Folder");
    }

    ImGui::SameLine();
    if (ImGui::Button("New Prefab"))
    {
        std::snprintf(NewPrefabName, sizeof(NewPrefabName), "NewPrefab");
        ImGui::OpenPopup("Create Prefab");
    }
}

void FEditorContentBrowserPanel::RenderPathBar()
{
    ImGui::TextDisabled("%s", CurrentDisplayPath.c_str());
}

void FEditorContentBrowserPanel::RenderItems()
{
    const ImVec2 TileSize(112.0f, 128.0f);
    const float Spacing = 10.0f;
    const float AvailableWidth = ImGui::GetContentRegionAvail().x;
    int32 Columns = static_cast<int32>((AvailableWidth + Spacing) / (TileSize.x + Spacing));
    if (Columns < 1)
    {
        Columns = 1;
    }
    const int32 ItemCount = static_cast<int32>(Items.size());
    const int32 RowCount = (ItemCount + Columns - 1) / Columns;
    const float RowHeight = TileSize.y + Spacing;
    const float ContentHeight = RowCount > 0 ? RowCount * RowHeight : 1.0f;

    ImGui::BeginChild("##ContentBrowserTileView", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    const ImVec2 Origin = ImGui::GetCursorScreenPos();
    const float ScrollY = ImGui::GetScrollY();
    const float ViewHeight = ImGui::GetWindowHeight();

    int32 FirstRow = static_cast<int32>(ScrollY / RowHeight) - 1;
    int32 LastRow = static_cast<int32>((ScrollY + ViewHeight) / RowHeight) + 2;
    if (FirstRow < 0)
    {
        FirstRow = 0;
    }
    if (LastRow > RowCount)
    {
        LastRow = RowCount;
    }

    ImGui::Dummy(ImVec2(Columns * (TileSize.x + Spacing), ContentHeight));

    for (int32 Row = FirstRow; Row < LastRow; ++Row)
    {
        for (int32 Column = 0; Column < Columns; ++Column)
        {
            const int32 Index = Row * Columns + Column;
            if (Index >= ItemCount)
            {
                break;
            }

            const ImVec2 TilePos(
                Origin.x + Column * (TileSize.x + Spacing),
                Origin.y + Row * RowHeight);
            ImGui::SetCursorScreenPos(TilePos);
            RenderItem(Items[Index]);
        }
    }

    ImGui::EndChild();
}

void FEditorContentBrowserPanel::RenderItem(const FContentItem& Item)
{
    DrawItemTile(Item, ImVec2(112.0f, 128.0f));
}

void FEditorContentBrowserPanel::DrawItemTile(const FContentItem& Item, const ImVec2& TileSize)
{
    const bool bSelected = !SelectedRelativePath.empty() && SelectedRelativePath == Item.RelativePath;

    ImGui::PushID(Item.RelativePath.c_str());

    ImGui::InvisibleButton("##ContentTile", TileSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool bHovered = ImGui::IsItemHovered();
    const bool bClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const bool bDoubleClicked = bHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    const ImVec2 CursorAfterTile = ImGui::GetCursorScreenPos();

    const ImVec2 TileMin = ImGui::GetItemRectMin();
    const ImVec2 TileMax = ImGui::GetItemRectMax();
    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    if (bClicked)
    {
        SelectedPath = Item.FullPath;
        SelectedRelativePath = Item.RelativePath;
    }

    if (Item.bDirectory && bDoubleClicked)
    {
        NavigateTo(Item.FullPath);
    }

    if (!Item.bDirectory && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
    {
        FContentBrowserDragPayload Payload;
        Payload.Type = Item.Type;
        std::snprintf(Payload.Path, sizeof(Payload.Path), "%s", Item.RelativePath.c_str());
        ImGui::SetDragDropPayload(ContentBrowserPayloadName, &Payload, sizeof(Payload));
        ImGui::TextUnformatted(Item.RelativePath.c_str());
        ImGui::EndDragDropSource();
    }

    RenderItemContextMenu(Item);

    const ImU32 BgColor = bSelected
                              ? IM_COL32(70, 105, 150, 125)
                              : (bHovered ? IM_COL32(255, 255, 255, 34) : IM_COL32(0, 0, 0, 0));
    const ImU32 BorderColor = bSelected
                                  ? IM_COL32(120, 170, 230, 210)
                                  : (bHovered ? IM_COL32(255, 255, 255, 65) : IM_COL32(0, 0, 0, 0));

    DrawList->AddRectFilled(TileMin, TileMax, BgColor, 6.0f);
    DrawList->AddRect(TileMin, TileMax, BorderColor, 6.0f);

    const ImVec2 ThumbMin(TileMin.x + 16.0f, TileMin.y + 10.0f);
    const ImVec2 ThumbMax(TileMax.x - 16.0f, TileMin.y + 78.0f);
    DrawTileThumbnail(Item, ThumbMin, ThumbMax);

    ImVec2 TextMin(TileMin.x + 6.0f, TileMin.y + 84.0f);
    ImVec2 TextMax(TileMax.x - 6.0f, TileMax.y - 6.0f);
    ImGui::SetCursorScreenPos(TextMin);
    ImGui::PushTextWrapPos(TextMax.x);
    ImGui::TextWrapped("%s", Item.Name.c_str());
    ImGui::PopTextWrapPos();
    ImGui::SetCursorScreenPos(CursorAfterTile);

    ImGui::PopID();
}

void FEditorContentBrowserPanel::DrawTileThumbnail(const FContentItem& Item, const ImVec2& Min, const ImVec2& Max)
{
    UTexture2D* Texture = nullptr;

    if (ShouldPreviewTexture(Item))
    {
        Texture = GetThumbnailTexture(Item.RelativePath);
    }
    else if (Item.Type == EContentBrowserAssetType::Material)
    {
        Texture = GetEditorIconTexture("Material.png");
    }

    if (Texture && Texture->GetSRV())
    {
        const float BoxW = Max.x - Min.x;
        const float BoxH = Max.y - Min.y;
        float DrawW = BoxW;
        float DrawH = BoxH;

        const float TexW = static_cast<float>(Texture->GetWidth() > 0 ? Texture->GetWidth() : 1);
        const float TexH = static_cast<float>(Texture->GetHeight() > 0 ? Texture->GetHeight() : 1);
        const float TexAspect = TexW / TexH;
        const float BoxAspect = BoxW / BoxH;

        if (TexAspect > BoxAspect)
        {
            DrawH = DrawW / TexAspect;
        }
        else
        {
            DrawW = DrawH * TexAspect;
        }

        const ImVec2 ImageMin(Min.x + (BoxW - DrawW) * 0.5f, Min.y + (BoxH - DrawH) * 0.5f);
        const ImVec2 ImageMax(ImageMin.x + DrawW, ImageMin.y + DrawH);

        ImGui::GetWindowDrawList()->AddImage(
            reinterpret_cast<ImTextureID>(Texture->GetSRV()),
            ImageMin,
            ImageMax);
        return;
    }

    DrawFallbackIcon(Item.Type, Min, Max);
}

void FEditorContentBrowserPanel::DrawFallbackIcon(EContentBrowserAssetType Type, const ImVec2& Min, const ImVec2& Max)
{
    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    const float W = Max.x - Min.x;
    const float H = Max.y - Min.y;
    const ImVec2 Center((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);

    if (Type == EContentBrowserAssetType::Folder)
    {
        const ImVec2 TabMin(Min.x + W * 0.12f, Min.y + H * 0.20f);
        const ImVec2 TabMax(Min.x + W * 0.48f, Min.y + H * 0.40f);
        const ImVec2 BodyMin(Min.x + W * 0.08f, Min.y + H * 0.32f);
        const ImVec2 BodyMax(Min.x + W * 0.92f, Min.y + H * 0.80f);
        DrawList->AddRectFilled(TabMin, TabMax, IM_COL32(218, 166, 62, 255), 4.0f);
        DrawList->AddRectFilled(BodyMin, BodyMax, IM_COL32(238, 190, 83, 255), 6.0f);
        DrawList->AddRect(BodyMin, BodyMax, IM_COL32(255, 225, 140, 180), 6.0f);
        return;
    }

    const ImU32 PaperColor = IM_COL32(230, 234, 240, 255);
    const ImU32 EdgeColor = IM_COL32(145, 155, 168, 255);
    const ImVec2 DocMin(Min.x + W * 0.23f, Min.y + H * 0.10f);
    const ImVec2 DocMax(Min.x + W * 0.77f, Min.y + H * 0.88f);
    const float Fold = W * 0.16f;

    DrawList->AddRectFilled(DocMin, DocMax, PaperColor, 4.0f);
    DrawList->AddTriangleFilled(
        ImVec2(DocMax.x - Fold, DocMin.y),
        ImVec2(DocMax.x, DocMin.y),
        ImVec2(DocMax.x, DocMin.y + Fold),
        IM_COL32(190, 198, 210, 255));
    DrawList->AddRect(DocMin, DocMax, EdgeColor, 4.0f);

    const char* Label = GetTypeLabel(Type);
    ImVec2 LabelMin(DocMin.x, Center.y - 8.0f);
    ImVec2 LabelMax(DocMax.x, Center.y + 10.0f);
    DrawCenteredText(Label, LabelMin, LabelMax, IM_COL32(55, 62, 74, 255));
}

void FEditorContentBrowserPanel::RenderBackgroundContextMenu()
{
    if (ImGui::BeginPopupContextWindow("ContentBrowserBackgroundContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        if (ImGui::MenuItem("Create Folder"))
        {
            std::snprintf(NewFolderName, sizeof(NewFolderName), "NewFolder");
            ImGui::OpenPopup("Create Folder");
        }
        if (ImGui::MenuItem("Create Prefab"))
        {
            std::snprintf(NewPrefabName, sizeof(NewPrefabName), "NewPrefab");
            ImGui::OpenPopup("Create Prefab");
        }
        if (ImGui::MenuItem("Refresh"))
        {
            bNeedsRefresh = true;
        }
        ImGui::EndPopup();
    }
}

void FEditorContentBrowserPanel::RenderItemContextMenu(const FContentItem& Item)
{
    if (ImGui::BeginPopupContextItem("ContentBrowserItemContext"))
    {
        ImGui::TextDisabled("%s", Item.Name.c_str());
        ImGui::Separator();
        if (Item.bDirectory && ImGui::MenuItem("Open"))
        {
            NavigateTo(Item.FullPath);
        }
        if (ImGui::MenuItem("Refresh"))
        {
            bNeedsRefresh = true;
        }
        ImGui::EndPopup();
    }
}

void FEditorContentBrowserPanel::RenderCreateFolderPopup()
{
    if (ImGui::BeginPopupModal("Create Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::InputText("Name", NewFolderName, sizeof(NewFolderName));

        if (ImGui::Button("Create", ImVec2(100.0f, 0.0f)))
        {
            if (CreateFolder(NewFolderName))
            {
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void FEditorContentBrowserPanel::RenderCreatePrefabPopup()
{
    if (ImGui::BeginPopupModal("Create Prefab", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::InputText("Name", NewPrefabName, sizeof(NewPrefabName));

        if (ImGui::Button("Create", ImVec2(100.0f, 0.0f)))
        {
            if (CreateEmptyPrefab(NewPrefabName))
            {
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

bool FEditorContentBrowserPanel::IsInsideContentRoot(const std::filesystem::path& Path) const
{
    const std::filesystem::path Root = NormalizePath(ContentRoot);
    const std::filesystem::path Target = NormalizePath(Path);

    auto RootIt = Root.begin();
    auto TargetIt = Target.begin();
    for (; RootIt != Root.end(); ++RootIt, ++TargetIt)
    {
        if (TargetIt == Target.end() || *RootIt != *TargetIt)
        {
            return false;
        }
    }
    return true;
}

FString FEditorContentBrowserPanel::MakeDisplayPath(const std::filesystem::path& Path) const
{
    const FString RelativePath = MakeContentRelativePath(Path);
    return RelativePath.empty() ? FString("Content") : FString("Content/") + RelativePath;
}

FString FEditorContentBrowserPanel::MakeContentRelativePath(const std::filesystem::path& Path) const
{
    std::filesystem::path Relative = Path.lexically_relative(ContentRoot);
    if (Relative.empty() || Relative == L".")
    {
        return "";
    }
    return FPaths::FromPath(Relative);
}

EContentBrowserAssetType FEditorContentBrowserPanel::GetAssetType(const std::filesystem::directory_entry& Entry) const
{
    std::error_code Ec;
    if (Entry.is_directory(Ec))
    {
        return EContentBrowserAssetType::Folder;
    }

    const std::filesystem::path Path = Entry.path();
    const FString Ext = ToLowerString(FPaths::FromPath(Path.extension()));

    if (Ext == ".scene")
        return EContentBrowserAssetType::Scene;
    if (Ext == ".lua")
        return EContentBrowserAssetType::LuaScript;
    if (Ext == ".json")
        return EContentBrowserAssetType::Material;
    if (Ext == ".obj" || Ext == ".fbx")
        return EContentBrowserAssetType::Model;
    if (Ext == ".png" || Ext == ".jpg" || Ext == ".jpeg" || Ext == ".dds")
        return EContentBrowserAssetType::Texture;
    if (IsPrefabExtension(Path))
        return EContentBrowserAssetType::Prefab;

    return EContentBrowserAssetType::Unknown;
}

const char* FEditorContentBrowserPanel::GetTypeLabel(EContentBrowserAssetType Type) const
{
    switch (Type)
    {
    case EContentBrowserAssetType::Folder:
        return "Folder";
    case EContentBrowserAssetType::Scene:
        return "Scene";
    case EContentBrowserAssetType::LuaScript:
        return "Lua Script";
    case EContentBrowserAssetType::Material:
        return "Material";
    case EContentBrowserAssetType::Model:
        return "Model";
    case EContentBrowserAssetType::Texture:
        return "Texture";
    case EContentBrowserAssetType::Prefab:
        return "Prefab";
    case EContentBrowserAssetType::Unknown:
    default:
        return "File";
    }
}

bool FEditorContentBrowserPanel::ShouldPreviewTexture(const FContentItem& Item) const
{
    if (Item.Type != EContentBrowserAssetType::Texture)
    {
        return false;
    }

    const FString Ext = ToLowerString(Item.Extension);
    return Ext == ".png" || Ext == ".jpg" || Ext == ".jpeg";
}

UTexture2D* FEditorContentBrowserPanel::GetThumbnailTexture(const FString& Path)
{
    auto It = ThumbnailCache.find(Path);
    if (It != ThumbnailCache.end())
    {
        return It->second;
    }

    if (!EditorEngine)
    {
        return nullptr;
    }

    UTexture2D* Texture = UTexture2D::LoadFromFile(FPaths::ContentRelativePath(Path), EditorEngine->GetRenderer().GetFD3DDevice().GetDevice());
    ThumbnailCache[Path] = Texture;
    return Texture;
}

UTexture2D* FEditorContentBrowserPanel::GetEditorIconTexture(const FString& FileName)
{
    auto It = EditorIconCache.find(FileName);
    if (It != EditorIconCache.end())
    {
        return It->second;
    }

    if (!EditorEngine)
    {
        return nullptr;
    }

    UTexture2D* Texture = UTexture2D::LoadFromFile(FPaths::EditorRelativePath("Icons/" + FileName), EditorEngine->GetRenderer().GetFD3DDevice().GetDevice());
    EditorIconCache[FileName] = Texture;
    return Texture;
}

bool FEditorContentBrowserPanel::CreateFolder(const FString& FolderName)
{
    if (FolderName.empty())
    {
        return false;
    }

    std::filesystem::path Target = NormalizePath(CurrentDirectory / FPaths::ToWide(FolderName));
    if (!IsInsideContentRoot(Target))
    {
        return false;
    }

    std::error_code Ec;
    std::filesystem::create_directories(Target, Ec);
    if (Ec)
    {
        UE_LOG(EditorUI, Warning, "Failed to create folder: %s", FPaths::FromPath(Target).c_str());
        return false;
    }

    bNeedsRefresh = true;
    return true;
}

bool FEditorContentBrowserPanel::CreateEmptyPrefab(const FString& FileName)
{
    if (FileName.empty())
    {
        return false;
    }

    std::filesystem::path TargetName = FPaths::ToWide(FileName);
    if (!IsPrefabExtension(TargetName))
    {
        TargetName += L".prefab";
    }

    std::filesystem::path Target = NormalizePath(CurrentDirectory / TargetName);
    if (!IsInsideContentRoot(Target))
    {
        return false;
    }

    std::error_code Ec;
    if (std::filesystem::exists(Target, Ec))
    {
        UE_LOG(EditorUI, Warning, "Prefab already exists: %s", FPaths::FromPath(Target).c_str());
        return false;
    }

    std::ofstream File(Target);
    if (!File.is_open())
    {
        UE_LOG(EditorUI, Warning, "Failed to create prefab: %s", FPaths::FromPath(Target).c_str());
        return false;
    }

    File << "{\n"
         << "  \"version\" : 1,\n"
         << "  \"rootActor\" : null\n"
         << "}\n";

    SelectedPath = Target;
    SelectedRelativePath = MakeContentRelativePath(Target);
    bNeedsRefresh = true;
    return true;
}
