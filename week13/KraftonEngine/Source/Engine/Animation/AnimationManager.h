#pragma once

#include "Core/CoreTypes.h"
#include "Asset/AssetRegistry.h"

class UAnimSequence;

struct FAnimationImportRequest
{
    FString SourceFbxPath;
    FString TargetSkeletonPath = "None";
    FString DestinationDirectory;
    bool    bAllowTargetExtraBones   = false;
    bool    bOverwriteExistingAssets = false;
};

class FAnimationManager
{
public:
    static FAnimationManager& Get();

    UAnimSequence* LoadAnimation(const FString& PackagePath);

    bool SaveAnimation(UAnimSequence* Sequence, const FString& PackagePath, const FString& SourcePath);

    // UI 편집 후 재 저장 — 기존 .uasset 의 SourcePath/Timestamp 메타데이터 보존.
    // Sequence 의 AssetPathFileName 을 그대로 PackagePath 로 사용.
    bool SaveAnimationPreservingMetadata(UAnimSequence* Sequence);

    bool ImportAnimationForSkeleton(const FAnimationImportRequest& Request, TArray<UAnimSequence*>* OutSequences = nullptr);

    // Content/ 하위를 스캔해 디스크의 AnimSequence .uasset 들을 목록에 채운다.
    // 시작 시/임포트 후 호출 — 런타임 Load/Save 만으로는 기존 파일이 목록에 안 잡힌다.
    void RefreshAvailableAnimations();

    const TArray<FAssetListItem>& GetAvailableAnimationFiles() const
    {
        return AvailableAnimationFiles;
    }

    static FString GetAnimationPath(const FString& SourcePath, const FString& AnimationName);
    static FString GetAnimationPathForSkeleton(const FString& SourcePath, const FString& AnimationName, const FString& TargetSkeletonPath);

private:
    FAnimationManager() = default;

private:
    TMap<FString, UAnimSequence*> AnimationCaches;
    TArray<FAssetListItem> AvailableAnimationFiles;
};
