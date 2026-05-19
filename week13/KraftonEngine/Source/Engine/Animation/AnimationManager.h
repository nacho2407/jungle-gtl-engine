#pragma once

#include "Core/CoreTypes.h"
#include "Asset/AssetRegistry.h"
#include "Animation/SkeletonTypes.h"

class UAnimSequence;
class UAnimMontage;

struct FAnimationImportRequest
{
    FString     SourceFbxPath;
    FString     TargetSkeletonPath = "None";
    FString     DestinationDirectory;
    bool        bAllowTargetExtraBones   = false;
    bool        bOverwriteExistingAssets = false;
    TSet<int32> SelectedAnimationStackIndices;
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

    bool SaveImportedAnimationsForSkeleton(
        const FString&            SourceFbxPath,
        const FReferenceSkeleton& SourceSkeleton,
        const FString&            TargetSkeletonPath,
        const FString&            DestinationDirectory,
        bool                      bAllowTargetExtraBones,
        bool                      bOverwriteExistingAssets,
        TArray<UAnimSequence*>&   ImportedSequences,
        TArray<UAnimSequence*>*   OutSequences = nullptr
        );

    // Content/ 하위를 스캔해 디스크의 AnimSequence .uasset 들을 목록에 채운다.
    // 시작 시/임포트 후 호출 — 런타임 Load/Save 만으로는 기존 파일이 목록에 안 잡힌다.
    void RefreshAvailableAnimations();

    const TArray<FAssetListItem>& GetAvailableAnimationFiles() const
    {
        return AvailableAnimationFiles;
    }

    static FString GetAnimationPath(const FString& SourcePath, const FString& AnimationName);
    static FString GetAnimationPathForSkeleton(const FString& SourcePath, const FString& AnimationName, const FString& TargetSkeletonPath);

    // ── Montage ──
    UAnimMontage* LoadMontage(const FString& PackagePath);
    bool          SaveMontage(UAnimMontage* Montage, const FString& PackagePath);
    bool          SaveMontagePreservingMetadata(UAnimMontage* Montage);

    // 새 비어있는 montage 를 메모리에 생성 (캐시 등록 X — caller 가 SaveMontage 호출 후 등록).
    UAnimMontage* CreateMontage(UAnimSequence* SourceSequence, const FString& MontageName);

    void RefreshAvailableMontages();
    const TArray<FAssetListItem>& GetAvailableMontageFiles() const { return AvailableMontageFiles; }

private:
    FAnimationManager() = default;

private:
    TMap<FString, UAnimSequence*> AnimationCaches;
    TArray<FAssetListItem> AvailableAnimationFiles;

    TMap<FString, UAnimMontage*>  MontageCaches;
    TArray<FAssetListItem>        AvailableMontageFiles;
};
