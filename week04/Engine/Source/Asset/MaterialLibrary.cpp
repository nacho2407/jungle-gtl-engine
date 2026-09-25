#include "Asset/MaterialLibrary.h"
#include "Asset/Asset.h"
#include "CoreUObject/Object.h"

namespace Engine::Asset
{
    void UMaterialLibrary::Initialize(const FSourceRecord& InSource,
                                      const FString& InLibraryName,
                                      TMap<FString, FAssetId> InMaterialRefs)
    {
        InitializeAssetMetadata(InSource);
        MaterialLibraryName = InLibraryName;
        MaterialRef = std::move(InMaterialRefs);
    }

    REGISTER_CLASS(Engine::Asset, UMaterialLibrary)
} // namespace Engine::Asset
