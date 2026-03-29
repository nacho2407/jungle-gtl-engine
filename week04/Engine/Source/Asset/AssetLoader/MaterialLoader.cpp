#include "Core/CoreMinimal.h"
#include "MaterialLoader.h"
#include "Asset/MaterialAsset.h" // UMaterialAsset 클래스 헤더
#include "Asset/Texture2DAsset.h"

#include <filesystem>
#include <sstream>
#include <cwctype>
#include <algorithm>

namespace fs = std::filesystem;

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
               "[MaterialLoader] Failed to parse .mtl file. Path = % s ",
               WidePathToUtf8(Source.NormalizedPath).c_str());

        return nullptr;
    }

    // 3. 에셋 래퍼 생성 및 반환
    Engine::Asset::UMaterialAsset* NewMatAsset = new Engine::Asset::UMaterialAsset();
    NewMatAsset->Initialize(Source, MatResource);

    if (AssetManager) // 포인터가 유효한지 확인
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

                const fs::path TexRelativePath(InMapPath.c_str());
                const fs::path FullTexPath = MtlDir / TexRelativePath;
                FWString       WideTexPath = FullTexPath.wstring();

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
                OutResource.Materials[CurrentMaterialName] = CurrentData;
            }

            std::string_view MatName = GetNextToken(LineView);
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
            std::string_view TexName = GetNextToken(LineView);
            if (!TexName.empty())
            {
                FWString AbsoluteTexturePath =
                    ResolveSiblingPath(Source.NormalizedPath, FString(TexName));
                CurrentData.AmbientMapPath = WidePathToUtf8(AbsoluteTexturePath);
            }
        }
        else if (Header == "map_Kd")
        {
            std::string_view TexName = GetNextToken(LineView);
            if (!TexName.empty())
            {
                FWString AbsoluteTexturePath =
                    ResolveSiblingPath(Source.NormalizedPath, FString(TexName));
                CurrentData.DiffuseMapPath = WidePathToUtf8(AbsoluteTexturePath);
            }
        }
        else if (Header == "map_Ns")
        {
            std::string_view TexName = GetNextToken(LineView);
            if (!TexName.empty())
            {
                FWString AbsoluteTexturePath =
                    ResolveSiblingPath(Source.NormalizedPath, FString(TexName));
                CurrentData.SpecularHighlightMapPath = WidePathToUtf8(AbsoluteTexturePath);
            }
        }
        else if (Header == "map_bump" || Header == "bump")
        {
            std::string_view TexName = GetNextToken(LineView);
            if (!TexName.empty())
            {
                FWString AbsoluteTexturePath =
                    ResolveSiblingPath(Source.NormalizedPath, FString(TexName));
                CurrentData.NormalMapPath = WidePathToUtf8(AbsoluteTexturePath);
            }
        }
    }

    // 루프가 끝난 후 마지막 재질 저장
    if (bHasMaterial && !CurrentMaterialName.empty())
    {
        OutResource.Materials[CurrentMaterialName] = CurrentData;
    }

    // 주석 처리된 로그 부분 유지
    // for (const auto& Pair : OutResource.Materials)
    // { ... }

    return !OutResource.Materials.empty();
    //if (!Source.bFileBytesLoaded || Source.FileBytes.empty())
    //    return false;

    //FString            FileString(reinterpret_cast<const char*>(Source.FileBytes.data()),
    //                              Source.FileBytes.size());
    //std::istringstream Stream(FileString);
    //FString            Line;

    //FString       CurrentMaterialName = "";
    //FMaterialData CurrentData;
    //bool          bHasMaterial = false;

    //while (std::getline(Stream, Line))
    //{
    //    if (Line.empty() || Line[0] == '#')
    //        continue;

    //    std::istringstream LineStream(Line);
    //    FString            Header;
    //    LineStream >> Header;

    //    if (Header == "newmtl")
    //    {
    //        // 이전 재질이 있었다면 맵에 저장
    //        if (bHasMaterial && !CurrentMaterialName.empty())
    //        {
    //            OutResource.Materials[CurrentMaterialName] = CurrentData;
    //        }

    //        LineStream >> CurrentMaterialName;
    //        CurrentData = FMaterialData(); // 새 재질 초기화
    //        bHasMaterial = true;
    //    }
    //    else if (Header == "Ka")
    //    {
    //        LineStream >> CurrentData.AmbientColor.X >> CurrentData.AmbientColor.Y >>
    //            CurrentData.AmbientColor.Z;
    //    }
    //    else if (Header == "Kd")
    //    {
    //        LineStream >> CurrentData.DiffuseColor.X >> CurrentData.DiffuseColor.Y >>
    //            CurrentData.DiffuseColor.Z;
    //    }
    //    else if (Header == "Ks")
    //    {
    //        LineStream >> CurrentData.SpecularColor.X >> CurrentData.SpecularColor.Y >>
    //            CurrentData.SpecularColor.Z;
    //    }
    //    else if (Header == "Ns")
    //    {
    //        LineStream >> CurrentData.SpecularHighlight;
    //    }
    //    else if (Header == "Ke")
    //    {
    //        LineStream >> CurrentData.EmissiveColor.X >> CurrentData.EmissiveColor.Y >>
    //            CurrentData.EmissiveColor.Z;
    //    }
    //    else if (Header == "Tf")
    //    {
    //        LineStream >> CurrentData.TransmissionFilter.X >> CurrentData.TransmissionFilter.Y >>
    //            CurrentData.TransmissionFilter.Z;
    //    }
    //    else if (Header == "Ni")
    //    {
    //        LineStream >> CurrentData.OpticalDensity;
    //    }
    //    else if (Header == "d")
    //    {
    //        LineStream >> CurrentData.Opacity;
    //    }
    //    else if (Header == "Tr") // .mtl 파일에 따라 d 대신 Tr(Transparency)를 쓰는 경우 대응
    //    {
    //        float Transparency;
    //        LineStream >> Transparency;
    //        CurrentData.Opacity = 1.0f - Transparency;
    //    }
    //    else if (Header == "illum")
    //    {
    //        LineStream >> CurrentData.IlluminationModel;
    //    }
    //    else if (Header == "map_Ka")
    //    {
    //        std::string TextureFilename;
    //        LineStream >> TextureFilename;
    //        FWString AbsoluteTexturePath =
    //            ResolveSiblingPath(Source.NormalizedPath, TextureFilename);
    //        CurrentData.AmbientMapPath = WidePathToUtf8(AbsoluteTexturePath);
    //    }
    //    else if (Header == "map_Kd")
    //    {
    //        FString TextureFilename;
    //        LineStream >> TextureFilename;

    //        // FontAtlasLoader에서 사용했던 방식처럼 Sibling Path로 절대 경로화
    //        FWString AbsoluteTexturePath =
    //            ResolveSiblingPath(Source.NormalizedPath, TextureFilename);
    //        CurrentData.DiffuseMapPath = WidePathToUtf8(AbsoluteTexturePath);
    //    }
    //    else if (Header == "map_Ns")
    //    {
    //        FString TextureFilename;
    //        LineStream >> TextureFilename;

    //        FWString AbsoluteTexturePath =
    //            ResolveSiblingPath(Source.NormalizedPath, TextureFilename);
    //        CurrentData.SpecularHighlightMapPath = WidePathToUtf8(AbsoluteTexturePath);
    //    }
    //    else if (Header == "map_bump" || Header == "bump")
    //    {
    //        std::string TextureFilename;
    //        LineStream >> TextureFilename;
    //        FWString AbsoluteTexturePath =
    //            ResolveSiblingPath(Source.NormalizedPath, TextureFilename);
    //        CurrentData.NormalMapPath = WidePathToUtf8(AbsoluteTexturePath);
    //    }
    //}

    //// 루프가 끝난 후 마지막 재질 저장
    //if (bHasMaterial && !CurrentMaterialName.empty())
    //{
    //    OutResource.Materials[CurrentMaterialName] = CurrentData;
    //}

    //// for (const auto& Pair : OutResource.Materials)
    ////{
    ////     const FMaterialData& Data = Pair.second;
    ////     if (Data.DiffuseMapPath.empty())
    ////     {
    ////         // 텍스처 경로가 없으면 에디터 콘솔에 노란색 경고 띄우기
    ////         UE_LOG(Asset, ELogVerbosity::Warning,
    ////                "[MaterialLoader] Material '%s' has no diffuse map.", Pair.first.c_str());
    ////     }
    //// }

    //return !OutResource.Materials.empty();
}

FWString FMaterialLoader::ResolveSiblingPath(const FWString& BaseFilePath,
                                             const FString&  RelativePath) const
{
    if (BaseFilePath.empty() || RelativePath.empty())
        return {};

    const fs::path BasePath(BaseFilePath);
    const fs::path Relative(RelativePath);

    const fs::path CombinedPath =
        Relative.is_absolute() ? Relative : (BasePath.parent_path() / Relative);

    std::error_code ErrorCode;
    fs::path        Normalized = fs::weakly_canonical(CombinedPath, ErrorCode);
    if (ErrorCode)
    {
        Normalized = CombinedPath.lexically_normal();
    }

    return Normalized.native();
}

FString FMaterialLoader::WidePathToUtf8(const FWString& Path) const
{
    const fs::path      FilePath(Path);
    const std::u8string Utf8Path = FilePath.u8string();
    return FString(reinterpret_cast<const char*>(Utf8Path.data()), Utf8Path.size());
}
