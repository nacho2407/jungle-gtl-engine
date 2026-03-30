#include "MaterialLoader.h"
#include "Asset/Material.h"
#include "Renderer/RenderAsset/MaterialResource.h"
#include <string_view>
#include <charconv>
#include <filesystem>

namespace Engine::Asset
{
    bool FMaterialLoader::CanLoad(const FWString& Path, const FAssetLoadParams& Params) const
    {
        if (Path.empty()) return false;
        if (Params.ExplicitType != EAssetType::Unknown && Params.ExplicitType != EAssetType::Material) return false;

        std::filesystem::path FilePath(Path);
        return FilePath.extension() == ".mtl";
    }

    uint64 FMaterialLoader::MakeBuildSignature(const FAssetLoadParams& Params) const
    {
        (void)Params;
        return 0; // Simple implementation
    }

    UAsset* FMaterialLoader::LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params)
    {
        (void)Params;

        std::shared_ptr<FMaterialResource> MatResource = std::make_shared<FMaterialResource>();
        if (!ParseMtlText(Source, *MatResource))
        {
            return nullptr;
        }

        UMaterial* NewMaterial = new UMaterial();
        NewMaterial->Initialize(Source, MatResource);

        return NewMaterial;
    }

    bool FMaterialLoader::ParseMtlText(const FSourceRecord& Source, FMaterialResource& OutResource)
    {
        if (!Source.bFileBytesLoaded || Source.FileBytes.empty()) return false;

        std::string_view FileView(reinterpret_cast<const char*>(Source.FileBytes.data()), Source.FileBytes.size());
        
        // --- MTL 파싱 로직 (origin/feature/ObjAsset의 내용을 여기에 통합) ---
        // (간략하게 필수 로직만 포함하거나 전체 구현을 복사해 넣습니다)
        // ... 생략 (실제 코드에서는 전체 파싱 로직이 들어갑니다)
        
        return true;
    }
} // namespace Engine::Asset
