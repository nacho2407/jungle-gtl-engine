#pragma once

#include "Core/CoreTypes.h"
#include "Asset/AssetRegistry.h"
#include "Mesh/Fbx/FbxImportTypes.h"

struct FFbxSceneImportRequest;
struct FAnimationImportRequest;

struct FFbxSceneImportDialogState
{
    bool                           bOpenRequested  = false;
    bool                           bCloseRequested = false;
    FString                        SourceFbxPath;
    bool                           bHasSkin                 = false;
    bool                           bImportSkeleton          = true;
    bool                           bImportSkin              = true;
    bool                           bImportAnimations        = true;
    bool                           bOverwriteExistingAssets = true;
    bool                           bAllowTargetExtraBones   = false;
    int32                          TargetSkeletonIndex      = -1;
    TArray<FAssetListItem>         TargetSkeletons;
    TArray<FFbxAnimationStackInfo> AnimationStacks;
    TArray<bool>                   AnimationStackSelected;
    FString                        Error;
};

struct FFbxAnimationImportDialogState
{
    bool                           bOpenRequested  = false;
    bool                           bCloseRequested = false;
    FString                        SourceFbxPath;
    TArray<FFbxAnimationStackInfo> AnimationStacks;
    TArray<bool>                   AnimationStackSelected;
    bool                           bOverwriteExistingAssets = false;
    FString                        Error;
};

enum class EFbxImportDialogResult : uint8
{
    None,
    Submitted,
    Cancelled,
};

class FFbxImportOptionsDialog
{
public:
    static void                   BeginSceneImport(FFbxSceneImportDialogState& State, const FString& FbxPath);
    static EFbxImportDialogResult RenderSceneImportPopup(
        const char*                 PopupId,
        FFbxSceneImportDialogState& State,
        FFbxSceneImportRequest&     OutRequest
        );

    static void                   BeginAnimationImport(FFbxAnimationImportDialogState& State, const FString& FbxPath);
    static EFbxImportDialogResult RenderAnimationImportPopup(
        const char*                     PopupId,
        FFbxAnimationImportDialogState& State,
        const FString&                  TargetSkeletonPath,
        FAnimationImportRequest&        OutRequest
        );

    static void RequestClose(FFbxSceneImportDialogState& State)
    {
        State.bCloseRequested = true;
    }

    static void RequestClose(FFbxAnimationImportDialogState& State)
    {
        State.bCloseRequested = true;
    }
};
