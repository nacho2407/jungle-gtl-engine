#pragma once

#include "Asset/IAssetLoader.h"

class UAnimSequence;

/* 이미 저장된 UAnimSequence 에셋을 로드 / 세이브하는 로더입니다. */
class FAnimSequenceAssetLoader : public IAssetLoader
{
public:
    UAnimSequence* Load(const FString& Path) const;
    bool Save(const FString& Path, const UAnimSequence* Sequence) const;

    bool SupportsExtension(const FString& Extension) const override;
    FString GetLoaderName() const override;
};
