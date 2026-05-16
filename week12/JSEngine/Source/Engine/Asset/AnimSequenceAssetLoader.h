#pragma once

#include "Asset/IAssetLoader.h"

class UAnimSequence;

class FAnimSequenceAssetLoader : public IAssetLoader
{
public:
    UAnimSequence* Load(const FString& Path) const;
    bool Save(const FString& Path, const UAnimSequence* Sequence) const;

    bool SupportsExtension(const FString& Extension) const override;
    FString GetLoaderName() const override;
};
