#pragma once

#include "Asset/IAssetLoader.h"
#include "Serialization/Archive.h"

struct FAnimSequenceAssetDescriptor
{
    FString AssetPath;
    FString SourceFbxPath;
    FString TargetSkeletalMeshPath;
    FString AnimStackName;

    bool IsValid() const;
    void Serialize(FArchive& Ar);
};

class FAnimSequenceAssetLoader : public IAssetLoader
{
public:
    bool Load(const FString& Path, FAnimSequenceAssetDescriptor& OutDescriptor) const;
    bool Save(const FString& Path, const FAnimSequenceAssetDescriptor& Descriptor) const;

    bool SupportsExtension(const FString& Extension) const override;
    FString GetLoaderName() const override;
};
