// 에디터 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"
#include "Editor/UI/EditorPanel.h"
#include "Math/Vector.h"

#include <filesystem>
#include <map>

struct ImVec2;

constexpr const char* ContentBrowserPayloadName = "CRASH_CONTENT_ASSET";

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
    CameraEffect,
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
    void RenderCreateCameraEffectPopup();
    void RenderCameraEffectEditor();

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
    bool CreateCameraEffect(const FString& FileName);
    bool OpenCameraEffectEditor(const FContentItem& Item);
    bool LoadCameraEffect(const std::filesystem::path& Path);
    bool SaveCameraEffect();

    struct FCurvePoint
    {
        float Time = 0.0f;
        float Value = 0.0f;
        float ArriveTangent = 0.0f;
        float LeaveTangent = 0.0f;
        bool bUseTangents = false;
    };

    struct FEditableCurve
    {
        TArray<FCurvePoint> Points;
        int32 SelectedIndex = -1;
    };

    struct FEditableCameraEffect
    {
        FString Type = "Vignette";
        float Duration = 0.25f;
        float FromValue = 0.0f;
        float ToValue = 1.0f;
        float LocationAmplitude = 5.0f;
        float RotationAmplitude = 1.0f;
        float Frequency = 30.0f;
        FVector4 Color = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
        float Radius = 0.75f;
        float Softness = 0.35f;
        FEditableCurve PrimaryCurve;
        FEditableCurve SecondaryCurve;
    };

    bool DrawCurveEditor(const char* Label, FEditableCurve& Curve, const ImVec2& Size);
    void DrawCameraEffectParameters();
    void ResetCameraEffectForType(const FString& Type);

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
    char NewCameraEffectName[128] = "NewCameraEffect";
    bool bCameraEffectEditorOpen = false;
    bool bCameraEffectDirty = false;
    FString ActiveCameraEffectCurve;
    std::filesystem::path EditingCameraEffectPath;
    FString EditingCameraEffectRelativePath;
    FEditableCameraEffect EditingCameraEffect;
};
