#include "Core/CoreMinimal.h"
#include "MaterialLoader.h"
#include "Asset/MaterialLibrary.h"
#include "Asset/Material.h"
#include "Asset/UAssetBinary.h"
#include "Core/Serialization/WindowsBinArchive.h"
#include "Asset/Texture2DAsset.h"

#include <filesystem>
#include <sstream>
#include <cwctype>
#include <algorithm>

namespace fs = std::filesystem;

namespace
{
    bool IsUAssetOfType(const FWString& Path, EUAssetBinaryType Expected)
    {
        const fs::path FilePath(Path);
        std::error_code ErrorCode;
        if (!fs::exists(FilePath, ErrorCode) || !fs::is_regular_file(FilePath, ErrorCode))
        {
            return false;
        }

        FWindowsBinReader Ar(FilePath);
        if (!Ar.IsValid())
        {
            return false;
        }

        FUAssetBinaryHeader Header;
        Header.Magic = 0;
        Header.Version = 0;
        uint32 TypeValue = 0;
        Header.AssetType = EUAssetBinaryType::Unknown;
        Header.SourceHash = 0;

        Ar << Header.Magic;
        Ar << Header.Version;
        Ar << TypeValue;
        Header.AssetType = static_cast<EUAssetBinaryType>(TypeValue);
        Ar << Header.SourceHash;

        if (Header.Magic != kUAssetMagic || Header.Version != kUAssetVersion)
        {
            return false;
        }

        return Header.AssetType == Expected;
    }

    FAssetId MakeMaterialAssetId(const FString& LibraryName, const FString& MaterialName)
    {
        FAssetId Id;
        Id.Type = EAssetType::Material;
        Id.PackageName = LibraryName;
        Id.ObjectName = MaterialName;
        return Id;
    }

    std::string_view TrimView(std::string_view View)
    {
        const size_t Begin = View.find_first_not_of(" \t\r");
        if (Begin == std::string_view::npos)
        {
            return {};
        }

        const size_t End = View.find_last_not_of(" \t\r");
        return View.substr(Begin, End - Begin + 1);
    }

    FString ParseMapPath(std::string_view View)
    {
        View = TrimView(View);
        if (View.empty())
        {
            return {};
        }

        // Common case: `map_Kd texture.png` or paths containing spaces.
        if (View.front() != '-')
        {
            return FString(View);
        }

        // Minimal fallback for option-prefixed map declarations such as:
        // `map_Kd -bm 1.0 texture.png`
        const size_t LastSeparator = View.find_last_of(" \t");
        if (LastSeparator == std::string_view::npos || LastSeparator + 1 >= View.size())
        {
            return {};
        }

        return FString(View.substr(LastSeparator + 1));
    }
} // namespace

bool FMaterialLoader::CanLoad(const FWString& Path, const FAssetLoadParams& Params) const
{
    if (Params.ExplicitType != EAssetType::Unknown && Params.ExplicitType != EAssetType::Material)
    {
        return false;
    }

    if (HasExtension(Path, {L".mtl"}))
    {
        return true;
    }

    if (HasExtension(Path, {L".uasset"}))
    {
        return IsUAssetOfType(Path, EUAssetBinaryType::Material);
    }

    return false;
}

EAssetType FMaterialLoader::GetAssetType() const { return EAssetType::Material; }

UAsset* FMaterialLoader::LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params)
{
    (void)Params;

    const bool bIsUAsset = HasExtension(Source.NormalizedPath, {L".uasset"});
    if (bIsUAsset)
    {
        FString MaterialName;
        FMaterial MaterialData;
        if (!ParseUAssetMaterial(Source, MaterialName, MaterialData))
        {
            UE_LOG(Asset, ELogVerbosity::Warning,
                   "[MaterialLoader] Failed to parse .uasset file. Path=%s ",
                   WidePathToUtf8(Source.NormalizedPath).c_str());
            return nullptr;
        }

        Engine::Asset::UMaterial* MatAsset = new Engine::Asset::UMaterial();
        if (AssetManager)
        {
            auto LoadTextureForMaterial = [&](const FString& InMapPath, bool bSRGB,
                                              bool        bIsNormalMap,
                                              const char* LogName,
                                              Engine::Asset::UMaterial::EMaterialTextureSlot Slot)
                -> UTexture2DAsset*
            {
                if (InMapPath.empty())
                {
                    return nullptr;
                }

                const FWString WideTexPath = NarrowPathToWidePath(InMapPath);
                if (WideTexPath.empty())
                {
                    UE_LOG(Asset, ELogVerbosity::Warning,
                           "[MaterialLoader] Failed to decode %s texture path. RawPath=%s",
                           LogName, InMapPath.c_str());
                    return nullptr;
                }

                FAssetLoadParams TexParams;
                TexParams.ExplicitType = EAssetType::Texture;
                TexParams.TextureSettings.bSRGB = bSRGB;
                TexParams.TextureSettings.bIsNormalMap = bIsNormalMap;

                UAsset* LoadedTex = AssetManager->Load(WideTexPath, TexParams);
                if (LoadedTex)
                {
                    UTexture2DAsset* TexAsset = static_cast<UTexture2DAsset*>(LoadedTex);
                    MatAsset->SetTextureDependency(Slot, TexAsset);
                    return TexAsset;
                }

                UE_LOG(Asset, ELogVerbosity::Warning,
                       "[MaterialLoader] Failed to load %s texture. Path=%s", LogName,
                       WidePathToUtf8(WideTexPath).c_str());
                return nullptr;
            };

            if (UTexture2DAsset* Tex = LoadTextureForMaterial(
                    MaterialData.DiffuseMapPath, true, false, "Diffuse",
                    Engine::Asset::UMaterial::EMaterialTextureSlot::Diffuse))
            {
                MaterialData.DiffuseTexture = Tex->GetResource();
            }
            if (UTexture2DAsset* Tex = LoadTextureForMaterial(
                    MaterialData.AmbientMapPath, false, false, "Ambient",
                    Engine::Asset::UMaterial::EMaterialTextureSlot::Ambient))
            {
                MaterialData.AmbientTexture = Tex->GetResource();
            }
            if (UTexture2DAsset* Tex = LoadTextureForMaterial(
                    MaterialData.SpecularHighlightMapPath, false, false, "Specular",
                    Engine::Asset::UMaterial::EMaterialTextureSlot::Specular))
            {
                MaterialData.SpecularTexture = Tex->GetResource();
            }
            if (UTexture2DAsset* Tex = LoadTextureForMaterial(
                    MaterialData.NormalMapPath, false, true, "Normal",
                    Engine::Asset::UMaterial::EMaterialTextureSlot::Normal))
            {
                MaterialData.NormalTexture = Tex->GetResource();
            }
        }

        if (MaterialName.empty())
        {
            const std::filesystem::path FilePath(Source.NormalizedPath);
            MaterialName = WidePathToUtf8(FilePath.stem().native());
        }
        MatAsset->Initialize(Source, MaterialData, MaterialName);

        if (AssetManager)
        {
            FAssetId Id;
            Id.Type = EAssetType::Material;
            Id.PackageName = WidePathToUtf8(Source.NormalizedPath);
            Id.ObjectName = MaterialName;
            AssetManager->RegisterAssetByIdAlias(Id, MatAsset);
        }

        return MatAsset;
    }

    // 1. 힙에 리소스 할당
    TMap<FString, FMaterial> ParsedMaterials;

    // 2. 파싱
    if (!ParseMtlText(Source, ParsedMaterials))
    {
        UE_LOG(Asset, ELogVerbosity::Warning,
               "[MaterialLoader] Failed to parse .mtl file. Path=%s ",
               WidePathToUtf8(Source.NormalizedPath).c_str());

        return nullptr;
    }

    // 3. 에셋 래퍼 생성 및 반환
    const std::filesystem::path MtlPath(Source.NormalizedPath);
    const FWString LibraryNameWide = MtlPath.stem().native();
    const FString  LibraryName = WidePathToUtf8(LibraryNameWide);
    Engine::Asset::UMaterialLibrary* NewMatAsset = new Engine::Asset::UMaterialLibrary();
    TMap<FString, FAssetId> MaterialRefs;
    Engine::Asset::UMaterialLibrary* ExistingLibrary = nullptr;

    if (AssetManager)
    {
        // MTL 파일의 디렉토리 경로 추출 (텍스처 상대 경로 조합용)
        const fs::path MtlFilePath(Source.NormalizedPath);
        const fs::path MtlDir = MtlFilePath.parent_path();

        FAssetKey LibraryKey;
        LibraryKey.Type = GetAssetType();
        LibraryKey.NormalizedPath = Source.NormalizedPath;
        LibraryKey.BuildSignature = MakeBuildSignature(Params);
        ExistingLibrary = Cast<Engine::Asset::UMaterialLibrary>(
            AssetManager->FindLoadedAsset(LibraryKey));

        for (auto& Pair : ParsedMaterials)
        {
            Engine::Asset::UMaterial* SubAsset = nullptr;
            if (!Pair.first.empty())
            {
                const FAssetId SubAssetId = MakeMaterialAssetId(LibraryName, Pair.first);
                MaterialRefs.insert_or_assign(Pair.first, SubAssetId);
                if (UAsset* ExistingAsset = AssetManager->FindAssetById(SubAssetId))
                {
                    SubAsset = Cast<Engine::Asset::UMaterial>(ExistingAsset);
                }
                if (!SubAsset)
                {
                    SubAsset = new Engine::Asset::UMaterial();
                    AssetManager->RegisterAssetById(SubAssetId, SubAsset);
                }
                else
                {
                    SubAsset->ClearTextureDependencies();
                }
            }

            auto LoadTextureForMaterial = [&](const FString& InMapPath, bool bSRGB,
                                              bool        bIsNormalMap,
                                              const char* LogName,
                                              Engine::Asset::UMaterial::EMaterialTextureSlot Slot)
                -> UTexture2DAsset*
            {
                if (InMapPath.empty())
                {
                    return nullptr;
                }

                // const fs::path TexturePath = fs::u8path(InMapPath);
                const FWString WideTexPath = NarrowPathToWidePath(InMapPath);
                if (WideTexPath.empty())
                {
                    UE_LOG(Asset, ELogVerbosity::Warning,
                           "[MaterialLoader] Failed to decode %s texture path. RawPath=%s",
                           LogName, InMapPath.c_str());
                    return nullptr;
                }

                FAssetLoadParams TexParams;
                TexParams.ExplicitType = EAssetType::Texture;
                TexParams.TextureSettings.bSRGB = bSRGB;
                TexParams.TextureSettings.bIsNormalMap = bIsNormalMap;

                UAsset* LoadedTex = AssetManager->Load(WideTexPath, TexParams);
                if (LoadedTex)
                {
                    UTexture2DAsset* TexAsset = static_cast<UTexture2DAsset*>(LoadedTex);
                    if (SubAsset)
                    {
                        SubAsset->SetTextureDependency(Slot, TexAsset);
                    }
                    return TexAsset;
                }
                // 로드 실패 시 에러 로그 출력 (LogName을 활용해 어떤 맵이 실패했는지 명시)
                UE_LOG(Asset, ELogVerbosity::Warning,
                       "[MaterialLoader] Failed to load %s texture. Path=%s", LogName,
                       WidePathToUtf8(WideTexPath).c_str());
                return nullptr;
            };
            FMaterial& CurrentMaterial = Pair.second;

            if (UTexture2DAsset* Tex = LoadTextureForMaterial(
                    CurrentMaterial.DiffuseMapPath, true, false, "Diffuse",
                    Engine::Asset::UMaterial::EMaterialTextureSlot::Diffuse))
            {
                CurrentMaterial.DiffuseTexture = Tex->GetResource();
            }
            if (UTexture2DAsset* Tex = LoadTextureForMaterial(
                    CurrentMaterial.AmbientMapPath, false, false, "Ambient",
                    Engine::Asset::UMaterial::EMaterialTextureSlot::Ambient))
            {
                CurrentMaterial.AmbientTexture = Tex->GetResource();
            }
            if (UTexture2DAsset* Tex = LoadTextureForMaterial(
                    CurrentMaterial.SpecularHighlightMapPath, false, false, "Specular",
                    Engine::Asset::UMaterial::EMaterialTextureSlot::Specular))
            {
                CurrentMaterial.SpecularTexture = Tex->GetResource();
            }
            if (UTexture2DAsset* Tex = LoadTextureForMaterial(
                    CurrentMaterial.NormalMapPath, false, true, "Normal",
                    Engine::Asset::UMaterial::EMaterialTextureSlot::Normal))
            {
                CurrentMaterial.NormalTexture = Tex->GetResource();
            }

            if (SubAsset)
            {
                SubAsset->Initialize(Source, CurrentMaterial, Pair.first);
            }
        }

        if (ExistingLibrary)
        {
            for (const auto& Pair : ExistingLibrary->GetMaterialRefs())
            {
                if (MaterialRefs.find(Pair.first) == MaterialRefs.end())
                {
                    AssetManager->UnregisterAssetById(Pair.second);
                }
            }
        }
    }
    NewMatAsset->Initialize(Source, LibraryName, std::move(MaterialRefs));
    return NewMatAsset;
}

bool FMaterialLoader::ParseMtlText(const FSourceRecord& Source,
                                   TMap<FString, FMaterial>& OutMaterials) const
{
    if (!Source.bFileBytesLoaded || Source.FileBytes.empty())
        return false;

    // 1. 파일 전체를 감싸는 View 생성 (메모리 복사 없음)
    std::string_view FileView(reinterpret_cast<const char*>(Source.FileBytes.data()),
                              Source.FileBytes.size());

    FString       CurrentMaterialName = "";
    FMaterial CurrentData;
    bool          bHasMaterial = false;

    // --- 파싱 헬퍼 함수들 ---
    auto GetNextToken = [](std::string_view& View) -> std::string_view
    {
        size_t First = View.find_first_not_of(" \t\r");
        if (First == std::string_view::npos)
            return {};
        View.remove_prefix(First);

        size_t           Last = View.find_first_of(" \t\r\n");
        std::string_view Token = View.substr(0, Last);
        if (Last != std::string_view::npos)
            View.remove_prefix(Last);
        else
            View = {};
        return Token;
    };

    auto ParseFloat = [](std::string_view Token, float& OutVal)
    {
        if (!Token.empty())
            std::from_chars(Token.data(), Token.data() + Token.size(), OutVal);
    };

    auto ParseInt = [](std::string_view Token, int32& OutVal)
    {
        if (!Token.empty())
            std::from_chars(Token.data(), Token.data() + Token.size(), OutVal);
    };

    // --- 메인 파싱 루프 ---
    size_t LineStart = 0;
    while (LineStart < FileView.length())
    {
        size_t LineEnd = FileView.find('\n', LineStart);
        if (LineEnd == std::string_view::npos)
            LineEnd = FileView.length();

        std::string_view LineView = FileView.substr(LineStart, LineEnd - LineStart);
        LineStart = LineEnd + 1;

        std::string_view Header = GetNextToken(LineView);
        if (Header.empty() || Header[0] == '#')
            continue;

        if (Header == "newmtl")
        {
            if (bHasMaterial && !CurrentMaterialName.empty())
            {
                if (OutMaterials.find(CurrentMaterialName) != OutMaterials.end())
                {
                    UE_LOG(Asset, ELogVerbosity::Warning,
                           "[MaterialLoader] Duplicate material name '%s' in .mtl '%s'. "
                           "Overwriting previous definition.",
                           CurrentMaterialName.c_str(),
                           WidePathToUtf8(Source.NormalizedPath).c_str());
                }
                OutMaterials[CurrentMaterialName] = CurrentData;
            }

            std::string_view MatName = GetNextToken(LineView);
            if (MatName.empty())
            {
                UE_LOG(Asset, ELogVerbosity::Warning,
                       "[MaterialLoader] Encountered empty newmtl name. Path=%s",
                       WidePathToUtf8(Source.NormalizedPath).c_str());
                bHasMaterial = false;
                CurrentMaterialName.clear();
                CurrentData = FMaterial();
                continue;
            }
            CurrentMaterialName = FString(MatName);
            CurrentData = FMaterial();
            bHasMaterial = true;
        }
        else if (Header == "Ka")
        {
            ParseFloat(GetNextToken(LineView), CurrentData.AmbientColor.X);
            ParseFloat(GetNextToken(LineView), CurrentData.AmbientColor.Y);
            ParseFloat(GetNextToken(LineView), CurrentData.AmbientColor.Z);
        }
        else if (Header == "Kd")
        {
            ParseFloat(GetNextToken(LineView), CurrentData.DiffuseColor.X);
            ParseFloat(GetNextToken(LineView), CurrentData.DiffuseColor.Y);
            ParseFloat(GetNextToken(LineView), CurrentData.DiffuseColor.Z);
        }
        else if (Header == "Ks")
        {
            ParseFloat(GetNextToken(LineView), CurrentData.SpecularColor.X);
            ParseFloat(GetNextToken(LineView), CurrentData.SpecularColor.Y);
            ParseFloat(GetNextToken(LineView), CurrentData.SpecularColor.Z);
        }
        else if (Header == "Ke")
        {
            ParseFloat(GetNextToken(LineView), CurrentData.EmissiveColor.X);
            ParseFloat(GetNextToken(LineView), CurrentData.EmissiveColor.Y);
            ParseFloat(GetNextToken(LineView), CurrentData.EmissiveColor.Z);
        }
        else if (Header == "Tf")
        {
            ParseFloat(GetNextToken(LineView), CurrentData.TransmissionFilter.X);
            ParseFloat(GetNextToken(LineView), CurrentData.TransmissionFilter.Y);
            ParseFloat(GetNextToken(LineView), CurrentData.TransmissionFilter.Z);
        }
        else if (Header == "Ns")
        {
            ParseFloat(GetNextToken(LineView), CurrentData.SpecularHighlight);
        }
        else if (Header == "Ni")
        {
            ParseFloat(GetNextToken(LineView), CurrentData.OpticalDensity);
        }
        else if (Header == "d")
        {
            ParseFloat(GetNextToken(LineView), CurrentData.Opacity);
        }
        else if (Header == "Tr")
        {
            float Transparency = 0.0f;
            ParseFloat(GetNextToken(LineView), Transparency);
            CurrentData.Opacity = 1.0f - Transparency;
        }
        else if (Header == "illum")
        {
            ParseInt(GetNextToken(LineView), CurrentData.IlluminationModel);
        }
        else if (Header == "map_Ka")
        {
            const FString TexName = ParseMapPath(LineView);
            if (!TexName.empty())
            {
                FWString AbsoluteTexturePath = ResolveSiblingPath(Source.NormalizedPath, TexName);
                CurrentData.AmbientMapPath = WidePathToUtf8(AbsoluteTexturePath);
            }
        }
        else if (Header == "map_Kd")
        {
            const FString TexName = ParseMapPath(LineView);
            if (!TexName.empty())
            {
                FWString AbsoluteTexturePath = ResolveSiblingPath(Source.NormalizedPath, TexName);
                CurrentData.DiffuseMapPath = WidePathToUtf8(AbsoluteTexturePath);
            }
        }
        else if (Header == "map_Ns")
        {
            const FString TexName = ParseMapPath(LineView);
            if (!TexName.empty())
            {
                FWString AbsoluteTexturePath = ResolveSiblingPath(Source.NormalizedPath, TexName);
                CurrentData.SpecularHighlightMapPath = WidePathToUtf8(AbsoluteTexturePath);
            }
        }
        else if (Header == "map_bump" || Header == "bump")
        {
            const FString TexName = ParseMapPath(LineView);
            if (!TexName.empty())
            {
                FWString AbsoluteTexturePath = ResolveSiblingPath(Source.NormalizedPath, TexName);
                CurrentData.NormalMapPath = WidePathToUtf8(AbsoluteTexturePath);
            }
        }
    }

    // 루프가 끝난 후 마지막 재질 저장
    if (bHasMaterial && !CurrentMaterialName.empty())
    {
        if (OutMaterials.find(CurrentMaterialName) != OutMaterials.end())
        {
            UE_LOG(Asset, ELogVerbosity::Warning,
                   "[MaterialLoader] Duplicate material name '%s' in .mtl '%s'. "
                   "Overwriting previous definition.",
                   CurrentMaterialName.c_str(), WidePathToUtf8(Source.NormalizedPath).c_str());
        }
        OutMaterials[CurrentMaterialName] = CurrentData;
    }
    return !OutMaterials.empty();
}

FWString FMaterialLoader::ResolveSiblingPath(const FWString& BaseFilePath,
                                             const FString&  RelativePath) const
{
    if (BaseFilePath.empty() || RelativePath.empty())
        return {};

    const FWString RelativeWidePath = NarrowPathToWidePath(RelativePath);
    if (RelativeWidePath.empty())
    {
        return {};
    }

    FWString NormalizedRelative = RelativeWidePath;
    NormalizePathSeparators(NormalizedRelative);

    if (IsAbsoluteWidePath(NormalizedRelative))
    {
        return NormalizedRelative;
    }

    FWString BaseDirectory = BaseFilePath;
    NormalizePathSeparators(BaseDirectory);

    const size_t LastSep = BaseDirectory.find_last_of(L"\\/");
    if (LastSep != FWString::npos)
    {
        BaseDirectory = BaseDirectory.substr(0, LastSep + 1);
    }
    else
    {
        BaseDirectory.clear();
    }

    return BaseDirectory + NormalizedRelative;
}


bool FMaterialLoader::ParseUAssetMaterial(const FSourceRecord& Source, FString& OutMaterialName,
                                           FMaterial& OutMaterial) const
{
    FWindowsBinReader Ar(Source.NormalizedPath);
    if (!Ar.IsValid())
    {
        return false;
    }

    FUAssetBinaryHeader Header;
    Header.Magic = 0;
    Header.Version = 0;
    uint32 TypeValue = 0;
    Header.AssetType = EUAssetBinaryType::Unknown;
    Header.SourceHash = 0;

    Ar << Header.Magic;
    Ar << Header.Version;
    Ar << TypeValue;
    Header.AssetType = static_cast<EUAssetBinaryType>(TypeValue);
    Ar << Header.SourceHash;

    if (Header.Magic != kUAssetMagic || Header.Version != kUAssetVersion ||
        Header.AssetType != EUAssetBinaryType::Material)
    {
        return false;
    }

    Ar << OutMaterialName;

    Ar.Serialize(&OutMaterial.AmbientColor, sizeof(FVector));
    Ar.Serialize(&OutMaterial.DiffuseColor, sizeof(FVector));
    Ar.Serialize(&OutMaterial.SpecularColor, sizeof(FVector));
    Ar.Serialize(&OutMaterial.EmissiveColor, sizeof(FVector));
    Ar.Serialize(&OutMaterial.TransmissionFilter, sizeof(FVector));

    Ar << OutMaterial.SpecularHighlight;
    Ar << OutMaterial.OpticalDensity;
    Ar << OutMaterial.Opacity;
    Ar << OutMaterial.IlluminationModel;

    Ar << OutMaterial.AmbientMapPath;
    Ar << OutMaterial.DiffuseMapPath;
    Ar << OutMaterial.SpecularHighlightMapPath;
    Ar << OutMaterial.NormalMapPath;

    Ar.Serialize(&OutMaterial.UVScrollSpeed, sizeof(FVector2));

    OutMaterial.AmbientTexture = nullptr;
    OutMaterial.DiffuseTexture = nullptr;
    OutMaterial.SpecularTexture = nullptr;
    OutMaterial.NormalTexture = nullptr;

    return !Ar.HasError();
}
