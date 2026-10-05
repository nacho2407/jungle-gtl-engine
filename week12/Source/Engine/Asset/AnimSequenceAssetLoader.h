#pragma once

#include "Asset/IAssetLoader.h"

class UAnimSequence;

struct FAnimSequenceAssetMetadata
{
    FString SourceFilePath;
    FString SourceStackName;
    FString PreviewMeshPath;
    FString SourceFileContentHash;
    uint64 SourceFileWriteTimeTicks = 0;
    uint64 SourceFileSizeBytes = 0;
    int32 TrackCount = 0;
    int32 NumberOfKeys = 0;
};

/* 이미 저장된 UAnimSequence 에셋을 로드 / 세이브하는 로더입니다. */
class FAnimSequenceAssetLoader : public IAssetLoader
{
public:
    UAnimSequence* Load(const FString& Path) const;
    bool LoadMetadata(const FString& Path, FAnimSequenceAssetMetadata& OutMetadata) const;
    bool HasValidBinaryCache(const FString& Path) const;
    bool Save(const FString& Path, const UAnimSequence* Sequence) const;

    bool SupportsExtension(const FString& Extension) const override;
};
