// 에디터 영역의 세부 동작을 구현합니다.
#include "Editor/UI/EditorContentBrowserPanel.h"

#include "Core/Logging/LogMacros.h"
#include "Editor/EditorEngine.h"
#include "Engine/CameraManage/CameraEffectAssetManager.h"
#include "Platform/Paths.h"
#include "Texture/Texture2D.h"

#include "ImGui/imgui.h"
#include "Profiling/Stats.h"
#include "SimpleJSON/json.hpp"

#include <d3d11.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <cstring>

namespace
{
bool IsPrefabExtension(const std::filesystem::path& Path)
{
    return Path.extension() == L".prefab" || Path.extension() == L".Prefab";
}

bool IsCameraEffectExtension(const std::filesystem::path& Path)
{
    return Path.extension() == L".ceffect" || Path.extension() == L".CEffect";
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

float Clamp01(float Value)
{
    if (Value < 0.0f)
    {
        return 0.0f;
    }
    if (Value > 1.0f)
    {
        return 1.0f;
    }
    return Value;
}

float ReadJsonNumber(json::JSON& Object)
{
    bool bOk = false;
    const double FloatValue = Object.ToFloat(bOk);
    if (bOk)
    {
        return static_cast<float>(FloatValue);
    }

    const long IntValue = Object.ToInt(bOk);
    return bOk ? static_cast<float>(IntValue) : 0.0f;
}

void WriteJsonFloat(std::ofstream& File, float Value)
{
    File << std::fixed << std::setprecision(6) << Value << std::defaultfloat;
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
    RenderCreateCameraEffectPopup();

    ImGui::End();

    RenderCameraEffectEditor();
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

    ImGui::SameLine();
    if (ImGui::Button("New Camera Effect"))
    {
        std::snprintf(NewCameraEffectName, sizeof(NewCameraEffectName), "NewCameraEffect");
        ImGui::OpenPopup("Create Camera Effect");
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
    else if (!Item.bDirectory && bDoubleClicked && Item.Type == EContentBrowserAssetType::CameraEffect)
    {
        OpenCameraEffectEditor(Item);
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
    else if (Item.Type == EContentBrowserAssetType::Model && ToLowerString(Item.Extension) == ".obj")
    {
        Texture = GetEditorIconTexture("obj_icon.png");
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
        if (ImGui::MenuItem("Create Camera Effect"))
        {
            std::snprintf(NewCameraEffectName, sizeof(NewCameraEffectName), "NewCameraEffect");
            ImGui::OpenPopup("Create Camera Effect");
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
        if (!Item.bDirectory && Item.Type == EContentBrowserAssetType::CameraEffect && ImGui::MenuItem("Open"))
        {
            OpenCameraEffectEditor(Item);
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

void FEditorContentBrowserPanel::RenderCreateCameraEffectPopup()
{
    if (ImGui::BeginPopupModal("Create Camera Effect", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::InputText("Name", NewCameraEffectName, sizeof(NewCameraEffectName));

        if (ImGui::Button("Create", ImVec2(100.0f, 0.0f)))
        {
            if (CreateCameraEffect(NewCameraEffectName))
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
    if (IsCameraEffectExtension(Path))
        return EContentBrowserAssetType::CameraEffect;

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
    case EContentBrowserAssetType::CameraEffect:
        return "Camera Effect";
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

bool FEditorContentBrowserPanel::CreateCameraEffect(const FString& FileName)
{
    if (FileName.empty())
    {
        return false;
    }

    std::filesystem::path TargetName = FPaths::ToWide(FileName);
    if (!IsCameraEffectExtension(TargetName))
    {
        TargetName += L".ceffect";
    }

    std::filesystem::path Target = NormalizePath(CurrentDirectory / TargetName);
    if (!IsInsideContentRoot(Target))
    {
        return false;
    }

    std::error_code Ec;
    if (std::filesystem::exists(Target, Ec))
    {
        UE_LOG(EditorUI, Warning, "Camera effect already exists: %s", FPaths::FromPath(Target).c_str());
        return false;
    }

    ResetCameraEffectForType("Vignette");
    EditingCameraEffectPath = Target;
    EditingCameraEffectRelativePath = MakeContentRelativePath(Target);
    bCameraEffectDirty = true;
    if (!SaveCameraEffect())
    {
        return false;
    }

    SelectedPath = Target;
    SelectedRelativePath = EditingCameraEffectRelativePath;
    bCameraEffectEditorOpen = true;
    bNeedsRefresh = true;
    return true;
}

bool FEditorContentBrowserPanel::OpenCameraEffectEditor(const FContentItem& Item)
{
    if (Item.Type != EContentBrowserAssetType::CameraEffect)
    {
        return false;
    }

    if (!LoadCameraEffect(Item.FullPath))
    {
        return false;
    }

    EditingCameraEffectPath = Item.FullPath;
    EditingCameraEffectRelativePath = Item.RelativePath;
    bCameraEffectEditorOpen = true;
    bCameraEffectDirty = false;
    return true;
}

void FEditorContentBrowserPanel::ResetCameraEffectForType(const FString& Type)
{
    EditingCameraEffect = FEditableCameraEffect{};
    EditingCameraEffect.Type = Type;

    EditingCameraEffect.PrimaryCurve.Points.push_back({0.0f, 0.0f});
    EditingCameraEffect.PrimaryCurve.Points.push_back({1.0f, 1.0f});

    if (Type == "Shake")
    {
        EditingCameraEffect.Duration = 0.2f;
        EditingCameraEffect.LocationAmplitude = 5.0f;
        EditingCameraEffect.RotationAmplitude = 1.0f;
        EditingCameraEffect.Frequency = 30.0f;
        EditingCameraEffect.SecondaryCurve.Points.push_back({0.0f, 1.0f});
        EditingCameraEffect.SecondaryCurve.Points.push_back({1.0f, 0.0f});
        EditingCameraEffect.PrimaryCurve.Points[0].Value = 1.0f;
        EditingCameraEffect.PrimaryCurve.Points[1].Value = 0.0f;
    }
    else if (Type == "Fade")
    {
        EditingCameraEffect.Duration = 0.25f;
        EditingCameraEffect.FromValue = 0.0f;
        EditingCameraEffect.ToValue = 1.0f;
        EditingCameraEffect.Color = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    else if (Type == "LetterBox")
    {
        EditingCameraEffect.Duration = 0.25f;
        EditingCameraEffect.FromValue = 0.0f;
        EditingCameraEffect.ToValue = 0.12f;
        EditingCameraEffect.AppearRatio = 0.5f;
        EditingCameraEffect.DisappearRatio = 0.5f;
    }
    else if (Type == "GammaCorrection")
    {
        EditingCameraEffect.Duration = 0.25f;
        EditingCameraEffect.FromValue = 1.0f;
        EditingCameraEffect.ToValue = 2.2f;
    }
    else
    {
        EditingCameraEffect.Type = "Vignette";
        EditingCameraEffect.Duration = 0.25f;
        EditingCameraEffect.FromValue = 0.0f;
        EditingCameraEffect.ToValue = 0.45f;
        EditingCameraEffect.Radius = 0.75f;
        EditingCameraEffect.Softness = 0.35f;
    }
}

bool FEditorContentBrowserPanel::LoadCameraEffect(const std::filesystem::path& Path)
{
    using json::JSON;

    std::ifstream File(Path);
    if (!File.is_open())
    {
        UE_LOG(EditorUI, Warning, "Failed to open camera effect: %s", FPaths::FromPath(Path).c_str());
        return false;
    }

    FString Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
    JSON Root = JSON::Load(Content);

    const FString Type = Root.hasKey("Type") ? Root["Type"].ToString() : FString("Vignette");
    ResetCameraEffectForType(Type);

    JSON* Section = Root.hasKey(Type.c_str()) ? &Root[Type.c_str()] : &Root;

    auto ReadFloat = [Section](const char* Key, float& Value)
    {
        if (Section->hasKey(Key))
        {
            Value = ReadJsonNumber((*Section)[Key]);
        }
    };

    auto ReadCurve = [Section](const char* Key, FEditableCurve& Curve)
    {
        if (!Section->hasKey(Key))
        {
            return;
        }

        Curve.Points.clear();
        for (auto& PointJson : (*Section)[Key].ArrayRange())
        {
            FCurvePoint Point;
            Point.Time = PointJson.hasKey("Time") ? ReadJsonNumber(PointJson["Time"]) : 0.0f;
            Point.Value = PointJson.hasKey("Value") ? ReadJsonNumber(PointJson["Value"]) : 0.0f;
            Point.ArriveTangent = PointJson.hasKey("ArriveTangent") ? ReadJsonNumber(PointJson["ArriveTangent"]) : 0.0f;
            Point.LeaveTangent = PointJson.hasKey("LeaveTangent") ? ReadJsonNumber(PointJson["LeaveTangent"]) : 0.0f;
            Point.bUseTangents = PointJson.hasKey("ArriveTangent") || PointJson.hasKey("LeaveTangent");
            Curve.Points.push_back({Clamp01(Point.Time), Clamp01(Point.Value)});
            Curve.Points.back().ArriveTangent = Point.ArriveTangent;
            Curve.Points.back().LeaveTangent = Point.LeaveTangent;
            Curve.Points.back().bUseTangents = Point.bUseTangents;
        }
        std::sort(Curve.Points.begin(), Curve.Points.end(), [](const FCurvePoint& A, const FCurvePoint& B)
        {
            return A.Time < B.Time;
        });
        Curve.SelectedIndex = -1;
    };

    ReadFloat("Duration", EditingCameraEffect.Duration);
    if (Type == "Shake")
    {
        ReadFloat("LocationAmplitude", EditingCameraEffect.LocationAmplitude);
        ReadFloat("RotationAmplitude", EditingCameraEffect.RotationAmplitude);
        ReadFloat("Frequency", EditingCameraEffect.Frequency);
        ReadCurve("LocationCurve", EditingCameraEffect.PrimaryCurve);
        ReadCurve("RotationCurve", EditingCameraEffect.SecondaryCurve);
    }
    else if (Type == "Fade")
    {
        ReadFloat("FromAmount", EditingCameraEffect.FromValue);
        ReadFloat("ToAmount", EditingCameraEffect.ToValue);
        if (Section->hasKey("Color"))
        {
            JSON& Color = (*Section)["Color"];
            EditingCameraEffect.Color.X = ReadJsonNumber(Color[0]);
            EditingCameraEffect.Color.Y = ReadJsonNumber(Color[1]);
            EditingCameraEffect.Color.Z = ReadJsonNumber(Color[2]);
            EditingCameraEffect.Color.W = ReadJsonNumber(Color[3]);
        }
        ReadCurve("AmountCurve", EditingCameraEffect.PrimaryCurve);
    }
    else if (Type == "LetterBox")
    {
        ReadFloat("FromAmount", EditingCameraEffect.FromValue);
        ReadFloat("ToAmount", EditingCameraEffect.ToValue);
        ReadFloat("AppearRatio", EditingCameraEffect.AppearRatio);
        ReadFloat("DisappearRatio", EditingCameraEffect.DisappearRatio);
        ReadCurve("AmountCurve", EditingCameraEffect.PrimaryCurve);
    }
    else if (Type == "GammaCorrection")
    {
        ReadFloat("FromGamma", EditingCameraEffect.FromValue);
        ReadFloat("ToGamma", EditingCameraEffect.ToValue);
        ReadCurve("GammaCurve", EditingCameraEffect.PrimaryCurve);
    }
    else
    {
        ReadFloat("FromIntensity", EditingCameraEffect.FromValue);
        ReadFloat("ToIntensity", EditingCameraEffect.ToValue);
        ReadFloat("Radius", EditingCameraEffect.Radius);
        ReadFloat("Softness", EditingCameraEffect.Softness);
        ReadCurve("IntensityCurve", EditingCameraEffect.PrimaryCurve);
    }

    bCameraEffectDirty = false;
    return true;
}

bool FEditorContentBrowserPanel::SaveCameraEffect()
{
    if (EditingCameraEffectPath.empty())
    {
        return false;
    }

    std::filesystem::create_directories(EditingCameraEffectPath.parent_path());
    std::ofstream File(EditingCameraEffectPath);
    if (!File.is_open())
    {
        UE_LOG(EditorUI, Warning, "Failed to save camera effect: %s", FPaths::FromPath(EditingCameraEffectPath).c_str());
        return false;
    }

    auto WriteCurve = [&File](const char* Name, const FEditableCurve& Curve, const char* Indent, bool bTrailingComma)
    {
        File << Indent << "\"" << Name << "\" : [\n";
        for (size_t Index = 0; Index < Curve.Points.size(); ++Index)
        {
            const FCurvePoint& Point = Curve.Points[Index];
            File << Indent << "  { \"Time\" : ";
            WriteJsonFloat(File, Point.Time);
            File << ", \"Value\" : ";
            WriteJsonFloat(File, Point.Value);
            if (Point.bUseTangents)
            {
                File << ", \"ArriveTangent\" : ";
                WriteJsonFloat(File, Point.ArriveTangent);
                File << ", \"LeaveTangent\" : ";
                WriteJsonFloat(File, Point.LeaveTangent);
            }
            File << " }";
            if (Index + 1 < Curve.Points.size())
            {
                File << ",";
            }
            File << "\n";
        }
        File << Indent << "]";
        if (bTrailingComma)
        {
            File << ",";
        }
        File << "\n";
    };

    File << "{\n";
    File << "  \"Type\" : \"" << EditingCameraEffect.Type << "\",\n";
    File << "  \"Version\" : 1,\n";

    if (EditingCameraEffect.Type == "Shake")
    {
        File << "  \"Shake\" : {\n";
        File << "    \"Duration\" : "; WriteJsonFloat(File, EditingCameraEffect.Duration); File << ",\n";
        File << "    \"LocationAmplitude\" : "; WriteJsonFloat(File, EditingCameraEffect.LocationAmplitude); File << ",\n";
        File << "    \"RotationAmplitude\" : "; WriteJsonFloat(File, EditingCameraEffect.RotationAmplitude); File << ",\n";
        File << "    \"Frequency\" : "; WriteJsonFloat(File, EditingCameraEffect.Frequency); File << ",\n";
        WriteCurve("LocationCurve", EditingCameraEffect.PrimaryCurve, "    ", true);
        WriteCurve("RotationCurve", EditingCameraEffect.SecondaryCurve, "    ", false);
        File << "  }\n";
    }
    else if (EditingCameraEffect.Type == "Fade")
    {
        File << "  \"Fade\" : {\n";
        File << "    \"Duration\" : "; WriteJsonFloat(File, EditingCameraEffect.Duration); File << ",\n";
        File << "    \"FromAmount\" : "; WriteJsonFloat(File, EditingCameraEffect.FromValue); File << ",\n";
        File << "    \"ToAmount\" : "; WriteJsonFloat(File, EditingCameraEffect.ToValue); File << ",\n";
        File << "    \"Color\" : [";
        WriteJsonFloat(File, EditingCameraEffect.Color.X); File << ", ";
        WriteJsonFloat(File, EditingCameraEffect.Color.Y); File << ", ";
        WriteJsonFloat(File, EditingCameraEffect.Color.Z); File << ", ";
        WriteJsonFloat(File, EditingCameraEffect.Color.W); File << "],\n";
        WriteCurve("AmountCurve", EditingCameraEffect.PrimaryCurve, "    ", false);
        File << "  }\n";
    }
    else if (EditingCameraEffect.Type == "LetterBox")
    {
        File << "  \"LetterBox\" : {\n";
        File << "    \"Duration\" : "; WriteJsonFloat(File, EditingCameraEffect.Duration); File << ",\n";
        File << "    \"FromAmount\" : "; WriteJsonFloat(File, EditingCameraEffect.FromValue); File << ",\n";
        File << "    \"ToAmount\" : "; WriteJsonFloat(File, EditingCameraEffect.ToValue); File << ",\n";
        File << "    \"AppearRatio\" : "; WriteJsonFloat(File, EditingCameraEffect.AppearRatio); File << ",\n";
        File << "    \"DisappearRatio\" : "; WriteJsonFloat(File, EditingCameraEffect.DisappearRatio); File << ",\n";
        WriteCurve("AmountCurve", EditingCameraEffect.PrimaryCurve, "    ", false);
        File << "  }\n";
    }
    else if (EditingCameraEffect.Type == "GammaCorrection")
    {
        File << "  \"GammaCorrection\" : {\n";
        File << "    \"Duration\" : "; WriteJsonFloat(File, EditingCameraEffect.Duration); File << ",\n";
        File << "    \"FromGamma\" : "; WriteJsonFloat(File, EditingCameraEffect.FromValue); File << ",\n";
        File << "    \"ToGamma\" : "; WriteJsonFloat(File, EditingCameraEffect.ToValue); File << ",\n";
        WriteCurve("GammaCurve", EditingCameraEffect.PrimaryCurve, "    ", false);
        File << "  }\n";
    }
    else
    {
        File << "  \"Vignette\" : {\n";
        File << "    \"Duration\" : "; WriteJsonFloat(File, EditingCameraEffect.Duration); File << ",\n";
        File << "    \"FromIntensity\" : "; WriteJsonFloat(File, EditingCameraEffect.FromValue); File << ",\n";
        File << "    \"ToIntensity\" : "; WriteJsonFloat(File, EditingCameraEffect.ToValue); File << ",\n";
        File << "    \"Radius\" : "; WriteJsonFloat(File, EditingCameraEffect.Radius); File << ",\n";
        File << "    \"Softness\" : "; WriteJsonFloat(File, EditingCameraEffect.Softness); File << ",\n";
        WriteCurve("IntensityCurve", EditingCameraEffect.PrimaryCurve, "    ", false);
        File << "  }\n";
    }

    File << "}\n";

    bCameraEffectDirty = false;
    bNeedsRefresh = true;
    FCameraEffectAssetManager::Invalidate(FPaths::FromPath(EditingCameraEffectPath));
    return true;
}

void FEditorContentBrowserPanel::RenderCameraEffectEditor()
{
    if (!bCameraEffectEditorOpen)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(620.0f, 620.0f), ImGuiCond_Once);
    FString Title = FString("Camera Effect Editor - ") + EditingCameraEffectRelativePath;
    if (bCameraEffectDirty)
    {
        Title += " *";
    }

    bool bOpen = bCameraEffectEditorOpen;
    if (!ImGui::Begin(Title.c_str(), &bOpen))
    {
        ImGui::End();
        bCameraEffectEditorOpen = bOpen;
        return;
    }

    ImGui::TextDisabled("%s", EditingCameraEffectRelativePath.c_str());
    ImGui::Separator();

    const char* Types[] = {"Shake", "Fade", "LetterBox", "GammaCorrection", "Vignette"};
    int32 CurrentType = 4;
    for (int32 Index = 0; Index < 5; ++Index)
    {
        if (EditingCameraEffect.Type == Types[Index])
        {
            CurrentType = Index;
            break;
        }
    }

    if (ImGui::Combo("Type", &CurrentType, Types, 5))
    {
        ResetCameraEffectForType(Types[CurrentType]);
        bCameraEffectDirty = true;
    }

    DrawCameraEffectParameters();

    ImGui::Separator();
    if (DrawCurveEditor(EditingCameraEffect.Type == "Shake" ? "Location Envelope" : "Value Curve", EditingCameraEffect.PrimaryCurve, ImVec2(0.0f, 190.0f)))
    {
        bCameraEffectDirty = true;
    }

    if (EditingCameraEffect.Type == "Shake")
    {
        if (DrawCurveEditor("Rotation Envelope", EditingCameraEffect.SecondaryCurve, ImVec2(0.0f, 190.0f)))
        {
            bCameraEffectDirty = true;
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Save", ImVec2(96.0f, 0.0f)))
    {
        SaveCameraEffect();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload", ImVec2(96.0f, 0.0f)))
    {
        LoadCameraEffect(EditingCameraEffectPath);
    }

    ImGui::End();
    bCameraEffectEditorOpen = bOpen;
}

void FEditorContentBrowserPanel::DrawCameraEffectParameters()
{
    if (ImGui::DragFloat("Duration", &EditingCameraEffect.Duration, 0.01f, 0.0f, 30.0f))
    {
        bCameraEffectDirty = true;
    }

    if (EditingCameraEffect.Type == "Shake")
    {
        bCameraEffectDirty |= ImGui::DragFloat("Location Amplitude", &EditingCameraEffect.LocationAmplitude, 0.1f, 0.0f, 1000.0f);
        bCameraEffectDirty |= ImGui::DragFloat("Rotation Amplitude", &EditingCameraEffect.RotationAmplitude, 0.1f, 0.0f, 360.0f);
        bCameraEffectDirty |= ImGui::DragFloat("Frequency", &EditingCameraEffect.Frequency, 0.1f, 0.0f, 240.0f);
        return;
    }

    const char* FromLabel = "From";
    const char* ToLabel = "To";
    if (EditingCameraEffect.Type == "Fade")
    {
        FromLabel = "From Amount";
        ToLabel = "To Amount";
    }
    else if (EditingCameraEffect.Type == "LetterBox")
    {
        FromLabel = "From Amount";
        ToLabel = "To Amount";
    }
    else if (EditingCameraEffect.Type == "GammaCorrection")
    {
        FromLabel = "From Gamma";
        ToLabel = "To Gamma";
    }
    else if (EditingCameraEffect.Type == "Vignette")
    {
        FromLabel = "From Intensity";
        ToLabel = "To Intensity";
    }

    bCameraEffectDirty |= ImGui::DragFloat(FromLabel, &EditingCameraEffect.FromValue, 0.01f, 0.0f, 8.0f);
    bCameraEffectDirty |= ImGui::DragFloat(ToLabel, &EditingCameraEffect.ToValue, 0.01f, 0.0f, 8.0f);

    if (EditingCameraEffect.Type == "Fade")
    {
        float Color[4] = {EditingCameraEffect.Color.X, EditingCameraEffect.Color.Y, EditingCameraEffect.Color.Z, EditingCameraEffect.Color.W};
        if (ImGui::ColorEdit4("Color", Color))
        {
            EditingCameraEffect.Color = FVector4(Color[0], Color[1], Color[2], Color[3]);
            bCameraEffectDirty = true;
        }
    }
    else if (EditingCameraEffect.Type == "LetterBox")
    {
        bCameraEffectDirty |= ImGui::DragFloat("Appear Ratio", &EditingCameraEffect.AppearRatio, 0.01f, 0.0f, 1.0f);
        bCameraEffectDirty |= ImGui::DragFloat("Disappear Ratio", &EditingCameraEffect.DisappearRatio, 0.01f, 0.0f, 1.0f);
    }
    else if (EditingCameraEffect.Type == "Vignette")
    {
        bCameraEffectDirty |= ImGui::DragFloat("Radius", &EditingCameraEffect.Radius, 0.01f, 0.0f, 2.0f);
        bCameraEffectDirty |= ImGui::DragFloat("Softness", &EditingCameraEffect.Softness, 0.01f, 0.001f, 2.0f);
    }
}

bool FEditorContentBrowserPanel::DrawCurveEditor(const char* Label, FEditableCurve& Curve, const ImVec2& Size)
{
    bool bChanged = false;
    ImGui::TextUnformatted(Label);
    const FString CurveId = Label;
    const bool bCurveActive = ActiveCameraEffectCurve == CurveId;

    ImVec2 CanvasSize = Size;
    if (CanvasSize.x <= 0.0f)
    {
        CanvasSize.x = ImGui::GetContentRegionAvail().x;
    }

    ImGui::InvisibleButton(Label, CanvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const ImVec2 Min = ImGui::GetItemRectMin();
    const ImVec2 Max = ImGui::GetItemRectMax();
    const bool bHovered = ImGui::IsItemHovered();
    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    DrawList->AddRectFilled(Min, Max, IM_COL32(24, 28, 34, 255), 4.0f);
    DrawList->AddRect(Min, Max, IM_COL32(95, 105, 120, 255), 4.0f);

    for (int32 Line = 1; Line < 4; ++Line)
    {
        const float X = Min.x + (Max.x - Min.x) * (static_cast<float>(Line) / 4.0f);
        const float Y = Min.y + (Max.y - Min.y) * (static_cast<float>(Line) / 4.0f);
        DrawList->AddLine(ImVec2(X, Min.y), ImVec2(X, Max.y), IM_COL32(255, 255, 255, 22));
        DrawList->AddLine(ImVec2(Min.x, Y), ImVec2(Max.x, Y), IM_COL32(255, 255, 255, 22));
    }

    auto ToScreen = [&](const FCurvePoint& Point)
    {
        return ImVec2(
            Min.x + Clamp01(Point.Time) * (Max.x - Min.x),
            Max.y - Clamp01(Point.Value) * (Max.y - Min.y));
    };

    auto GetTangent = [&](size_t Index)
    {
        if (Curve.Points.empty())
        {
            return 0.0f;
        }

        if (Curve.Points[Index].bUseTangents)
        {
            return Curve.Points[Index].LeaveTangent;
        }

        if (Index == 0)
        {
            if (Curve.Points.size() < 2)
            {
                return 0.0f;
            }
            const float Range = Curve.Points[1].Time - Curve.Points[0].Time;
            return Range > 0.0001f ? (Curve.Points[1].Value - Curve.Points[0].Value) / Range : 0.0f;
        }

        if (Index + 1 >= Curve.Points.size())
        {
            const float Range = Curve.Points[Index].Time - Curve.Points[Index - 1].Time;
            return Range > 0.0001f ? (Curve.Points[Index].Value - Curve.Points[Index - 1].Value) / Range : 0.0f;
        }

        const float Range = Curve.Points[Index + 1].Time - Curve.Points[Index - 1].Time;
        return Range > 0.0001f ? (Curve.Points[Index + 1].Value - Curve.Points[Index - 1].Value) / Range : 0.0f;
    };

    auto ToScreenValue = [&](float Time, float Value)
    {
        return ImVec2(
            Min.x + Clamp01(Time) * (Max.x - Min.x),
            Max.y - Clamp01(Value) * (Max.y - Min.y));
    };

    std::sort(Curve.Points.begin(), Curve.Points.end(), [](const FCurvePoint& A, const FCurvePoint& B)
    {
        return A.Time < B.Time;
    });

    for (size_t Index = 1; Index < Curve.Points.size(); ++Index)
    {
        const FCurvePoint& Prev = Curve.Points[Index - 1];
        const FCurvePoint& Next = Curve.Points[Index];
        const float Range = Next.Time - Prev.Time;
        const float PrevTangent = Prev.bUseTangents ? Prev.LeaveTangent : GetTangent(Index - 1);
        const float NextTangent = Next.bUseTangents ? Next.ArriveTangent : GetTangent(Index);
        const ImVec2 P0 = ToScreen(Prev);
        const ImVec2 P3 = ToScreen(Next);
        const ImVec2 P1 = ToScreenValue(Prev.Time + Range / 3.0f, Prev.Value + PrevTangent * Range / 3.0f);
        const ImVec2 P2 = ToScreenValue(Next.Time - Range / 3.0f, Next.Value - NextTangent * Range / 3.0f);
        DrawList->AddBezierCubic(P0, P1, P2, P3, IM_COL32(90, 180, 255, 255), 2.0f, 24);
    }

    const ImVec2 Mouse = ImGui::GetIO().MousePos;
    int32 HoveredPoint = -1;
    for (int32 Index = 0; Index < static_cast<int32>(Curve.Points.size()); ++Index)
    {
        const ImVec2 P = ToScreen(Curve.Points[Index]);
        const float Dx = Mouse.x - P.x;
        const float Dy = Mouse.y - P.y;
        const bool bPointHovered = bHovered && (Dx * Dx + Dy * Dy) <= 64.0f;
        if (bPointHovered)
        {
            HoveredPoint = Index;
        }

        const ImU32 Color = Curve.SelectedIndex == Index ? IM_COL32(255, 210, 80, 255)
                            : (bPointHovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(90, 180, 255, 255));
        DrawList->AddCircleFilled(P, 4.5f, Color);
    }

    if (bHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        FCurvePoint NewPoint;
        NewPoint.Time = Clamp01((Mouse.x - Min.x) / (Max.x - Min.x));
        NewPoint.Value = Clamp01((Max.y - Mouse.y) / (Max.y - Min.y));
        Curve.Points.push_back(NewPoint);
        Curve.SelectedIndex = static_cast<int32>(Curve.Points.size()) - 1;
        ActiveCameraEffectCurve = CurveId;
        bChanged = true;
    }
    else if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        Curve.SelectedIndex = HoveredPoint;
        ActiveCameraEffectCurve = HoveredPoint >= 0 ? CurveId : FString();
    }

    if (bCurveActive && Curve.SelectedIndex >= 0 && Curve.SelectedIndex < static_cast<int32>(Curve.Points.size()) && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        FCurvePoint& Point = Curve.Points[Curve.SelectedIndex];
        Point.Time = Clamp01((Mouse.x - Min.x) / (Max.x - Min.x));
        Point.Value = Clamp01((Max.y - Mouse.y) / (Max.y - Min.y));
        bChanged = true;
    }

    if (bCurveActive && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        ActiveCameraEffectCurve.clear();
    }

    if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && HoveredPoint >= 0 && Curve.Points.size() > 2)
    {
        Curve.Points.erase(Curve.Points.begin() + HoveredPoint);
        Curve.SelectedIndex = -1;
        bChanged = true;
    }

    if (Curve.SelectedIndex >= 0 && Curve.SelectedIndex < static_cast<int32>(Curve.Points.size()))
    {
        FCurvePoint& Point = Curve.Points[Curve.SelectedIndex];
        ImGui::PushID(Label);
        ImGui::SetNextItemWidth(110.0f);
        bChanged |= ImGui::DragFloat("Time", &Point.Time, 0.005f, 0.0f, 1.0f);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        bChanged |= ImGui::DragFloat("Value", &Point.Value, 0.005f, 0.0f, 1.0f);
        Point.Time = Clamp01(Point.Time);
        Point.Value = Clamp01(Point.Value);
        ImGui::PopID();
    }
    else
    {
        ImGui::TextDisabled("Double-click to add a point. Drag points to edit. Right-click a point to delete it.");
    }

    return bChanged;
}
