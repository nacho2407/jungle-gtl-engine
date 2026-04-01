#include "Core/CoreMinimal.h"
#include "MaterialLoader.h"
#include "Asset/Material.h"
#include "Asset/Texture2DAsset.h"

#include <filesystem>
#include <sstream>
#include <cwctype>
#include <algorithm>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace fs = std::filesystem;

namespace
{
    FWString NarrowPathToWidePath(const FString& NarrowPath)
    {
        if (NarrowPath.empty())
        {
            return {};
        }

        auto ConvertWithCodePage = [&](UINT CodePage, DWORD Flags) -> FWString
        {
            const int32 RequiredChars =
                MultiByteToWideChar(CodePage, Flags, NarrowPath.data(),
                                    static_cast<int>(NarrowPath.size()), nullptr, 0);
            if (RequiredChars <= 0)
            {
                return {};
            }

            FWString WidePath(static_cast<size_t>(RequiredChars), L'\0');
            const int32 ConvertedChars =
                MultiByteToWideChar(CodePage, Flags, NarrowPath.data(),
                                    static_cast<int>(NarrowPath.size()), WidePath.data(),
                                    RequiredChars);
            if (ConvertedChars <= 0)
            {
                return {};
            }

            return WidePath;
        };

        FWString WidePath = ConvertWithCodePage(CP_UTF8, MB_ERR_INVALID_CHARS);
        if (!WidePath.empty())
        {
            return WidePath;
        }

        WidePath = ConvertWithCodePage(CP_ACP, 0);
        if (!WidePath.empty())
        {
            return WidePath;
        }

        return ConvertWithCodePage(CP_OEMCP, 0);
    }

    bool IsAbsoluteWidePath(const FWString& Path)
    {
        if (Path.size() >= 2 && Path[1] == L':')
        {
            return true;
        }

        return Path.size() >= 2 && Path[0] == L'\\' && Path[1] == L'\\';
    }

    void NormalizePathSeparators(FWString& Path)
    {
        std::replace(Path.begin(), Path.end(), L'/', L'\\');
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
    if (Path.empty())
        return false;

    if (Params.ExplicitType != EAssetType::Unknown && Params.ExplicitType != EAssetType::Material)
    {
        return false;
    }

    const fs::path FilePath(Path);
    FWString       Extension = FilePath.extension().native();
    std::transform(Extension.begin(), Extension.end(), Extension.begin(),
                   [](wchar_t Ch) { return static_cast<wchar_t>(std::towlower(Ch)); });

    return Extension == L".mtl";
}

EAssetType FMaterialLoader::GetAssetType() const { return EAssetType::Material; }

uint64 FMaterialLoader::MakeBuildSignature(const FAssetLoadParams& Params) const
{
    (void)Params;
    uint64 Hash = 14695981039346656037ull;
    Hash ^= static_cast<uint64>(GetAssetType());
    Hash *= 1099511628211ull;
    return Hash;
}

UAsset* FMaterialLoader::LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params)
{
    (void)Params;

    // 1. 힙에 리소스 할당
    std::shared_ptr<FMaterialResource> MatResource = std::make_shared<FMaterialResource>();
    MatResource->Reset();

    // 2. 파싱
    if (!ParseMtlText(Source, *MatResource))
    {
        UE_LOG(Asset, ELogVerbosity::Warning,
               "[MaterialLoader] Failed to parse .mtl file. Path=%s ",
               WidePathToUtf8(Source.NormalizedPath).c_str());

        return nullptr;
    }

    // 3. 에셋 래퍼 생성 및 반환
    Engine::Asset::UMaterial* NewMatAsset = new Engine::Asset::UMaterial();
    NewMatAsset->Initialize(Source, MatResource);

    const std::filesystem::path MtlPath(Source.NormalizedPath);
    const FWString LibraryNameWide = MtlPath.stem().native();
    const FString  LibraryName = WidePathToUtf8(LibraryNameWide);
    NewMatAsset->SetMaterialLibraryName(LibraryName);

    if (AssetManager)
    {
        // MTL 파일의 디렉토리 경로 추출 (텍스처 상대 경로 조합용)
        const fs::path MtlFilePath(Source.NormalizedPath);
        const fs::path MtlDir = MtlFilePath.parent_path();

        for (auto& Pair : MatResource->Materials)
        {
            auto LoadTextureForMaterial = [&](const FString& InMapPath, bool bSRGB,
                                              bool        bIsNormalMap,
                                              const char* LogName) -> FTextureResource*
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
                    NewMatAsset->AddTextureDependency(TexAsset);
                    return TexAsset->GetResource();
                }
                // 로드 실패 시 에러 로그 출력 (LogName을 활용해 어떤 맵이 실패했는지 명시)
                UE_LOG(Asset, ELogVerbosity::Warning,
                       "[MaterialLoader] Failed to load %s texture. Path=%s", LogName,
                       WidePathToUtf8(WideTexPath).c_str());
                return nullptr;
            };
            FMaterialData& CurrentMaterial = Pair.second;

            CurrentMaterial.DiffuseTexture =
                LoadTextureForMaterial(CurrentMaterial.DiffuseMapPath, true, false, "Diffuse");
            CurrentMaterial.AmbientTexture =
                LoadTextureForMaterial(CurrentMaterial.AmbientMapPath, false, false, "Ambient");
            CurrentMaterial.SpecularTexture = LoadTextureForMaterial(
                CurrentMaterial.SpecularHighlightMapPath, false, false, "Specular");
            CurrentMaterial.NormalTexture =
                LoadTextureForMaterial(CurrentMaterial.NormalMapPath, false, true, "Normal");
        }
    }
    return NewMatAsset;
}

bool FMaterialLoader::ParseMtlText(const FSourceRecord& Source,
                                   FMaterialResource&   OutResource) const
{
    if (!Source.bFileBytesLoaded || Source.FileBytes.empty())
        return false;

    // 1. 파일 전체를 감싸는 View 생성 (메모리 복사 없음)
    std::string_view FileView(reinterpret_cast<const char*>(Source.FileBytes.data()),
                              Source.FileBytes.size());

    FString       CurrentMaterialName = "";
    FMaterialData CurrentData;
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
                if (OutResource.Materials.find(CurrentMaterialName) != OutResource.Materials.end())
                {
                    UE_LOG(Asset, ELogVerbosity::Warning,
                           "[MaterialLoader] Duplicate material name '%s' in .mtl '%s'. "
                           "Overwriting previous definition.",
                           CurrentMaterialName.c_str(),
                           WidePathToUtf8(Source.NormalizedPath).c_str());
                }
                OutResource.Materials[CurrentMaterialName] = CurrentData;
            }

            std::string_view MatName = GetNextToken(LineView);
            if (MatName.empty())
            {
                UE_LOG(Asset, ELogVerbosity::Warning,
                       "[MaterialLoader] Encountered empty newmtl name. Path=%s",
                       WidePathToUtf8(Source.NormalizedPath).c_str());
                bHasMaterial = false;
                CurrentMaterialName.clear();
                CurrentData = FMaterialData();
                continue;
            }
            CurrentMaterialName = FString(MatName);
            CurrentData = FMaterialData();
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
        if (OutResource.Materials.find(CurrentMaterialName) != OutResource.Materials.end())
        {
            UE_LOG(Asset, ELogVerbosity::Warning,
                   "[MaterialLoader] Duplicate material name '%s' in .mtl '%s'. "
                   "Overwriting previous definition.",
                   CurrentMaterialName.c_str(), WidePathToUtf8(Source.NormalizedPath).c_str());
        }
        OutResource.Materials[CurrentMaterialName] = CurrentData;
    }

    // 주석 처리된 로그 부분 유지
    // for (const auto& Pair : OutResource.Materials)
    // { ... }

    return !OutResource.Materials.empty();
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

    const fs::path BasePath(BaseFilePath);
    FWString BaseDirectory = BasePath.parent_path().native();
    NormalizePathSeparators(BaseDirectory);

    if (!BaseDirectory.empty() && BaseDirectory.back() != L'\\')
    {
        BaseDirectory += L'\\';
    }

    return BaseDirectory + NormalizedRelative;
}

FString FMaterialLoader::WidePathToUtf8(const FWString& Path) const
{
    const fs::path      FilePath(Path);
    const std::u8string Utf8Path = FilePath.u8string();
    return FString(reinterpret_cast<const char*>(Utf8Path.data()), Utf8Path.size());
}
