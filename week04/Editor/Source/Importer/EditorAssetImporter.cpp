#include "Importer/EditorAssetImporter.h"

#include "Asset/UAssetBinary.h"
#include "Asset/AssetManager.h"
#include "Core/Path.h"
#include "Core/Serialization/WindowsBinArchive.h"
#include "Renderer/Types/VertexTypes.h"
#include "Core/CoreMinimal.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cwctype>
#include <algorithm>
#include <string_view>
#include <charconv>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace
{
    namespace fs = std::filesystem;

    FString ParseMapPath(std::string_view View)
    {
        const size_t Begin = View.find_first_not_of(" \t\r");
        if (Begin == std::string_view::npos)
        {
            return {};
        }

        const size_t End = View.find_last_not_of(" \t\r");
        View = View.substr(Begin, End - Begin + 1);

        if (View.empty())
        {
            return {};
        }

        if (View.front() != '-')
        {
            return FString(View);
        }

        const size_t LastSeparator = View.find_last_of(" \t");
        if (LastSeparator == std::string_view::npos || LastSeparator + 1 >= View.size())
        {
            return {};
        }

        return FString(View.substr(LastSeparator + 1));
    }
}

FEditorAssetImporter::FEditorAssetImporter(FD3D11RHI* InRHI, UAssetManager* InAssetManager)
    : RHI(InRHI), AssetManager(InAssetManager)
{
    (void)RHI;
}

bool FEditorAssetImporter::ImportFile(const FWString& Path)
{
    if (Path.empty())
    {
        return false;
    }

    fs::path FilePath(Path);
    FWString Extension = FilePath.extension().native();
    std::transform(Extension.begin(), Extension.end(), Extension.begin(),
                   [](wchar_t Ch) { return static_cast<wchar_t>(std::towlower(Ch)); });

    if (Extension == L".obj")
    {
        return ImportObj(Path);
    }

    if (Extension == L".mtl")
    {
        return ImportMtl(Path);
    }

    return false;
}

bool FEditorAssetImporter::ImportObj(const FWString& ObjPath)
{
    FSourceData Source;
    if (!LoadSource(ObjPath, Source))
    {
        return false;
    }

    FStaticMesh Mesh;
    Mesh.Reset();

    TArray<FMeshVertexPNCT> Vertices;
    if (!ParseObjText(Source, Mesh, Vertices))
    {
        return false;
    }

    // If mtllib exists, import materials and build asset path table
    TMap<FString, FString> MaterialAssetMap;
    if (!Mesh.MaterialLibraryPath.empty())
    {
        const fs::path ObjFilePath(Source.NormalizedPath);
        const fs::path ObjDir = ObjFilePath.parent_path();

        FWString MtlRelativeWidePath = NarrowPathToWidePath(Mesh.MaterialLibraryPath);
        if (!MtlRelativeWidePath.empty())
        {
            NormalizePathSeparators(MtlRelativeWidePath);

            FWString WideMtlPath;
            if (IsAbsoluteWidePath(MtlRelativeWidePath))
            {
                WideMtlPath = MtlRelativeWidePath;
            }
            else
            {
                FWString ObjDirectory = ObjDir.native();
                NormalizePathSeparators(ObjDirectory);
                if (!ObjDirectory.empty() && ObjDirectory.back() != L'\\')
                {
                    ObjDirectory += L'\\';
                }
                WideMtlPath = ObjDirectory + MtlRelativeWidePath;
            }

            // Import mtl and capture generated paths
            if (ImportMtl(WideMtlPath))
            {
                // Re-scan the mtl to build deterministic paths
                FSourceData MtlSource;
                if (LoadSource(WideMtlPath, MtlSource))
                {
                    TMap<FString, FMaterial> Materials;
                    if (ParseMtlText(MtlSource, Materials))
                    {
                        for (const auto& Pair : Materials)
                        {
                            const std::filesystem::path OutPath =
                                MakeMaterialUAssetPath(fs::path(WideMtlPath), Pair.first);
                            MaterialAssetMap[Pair.first] =
                                WidePathToUtf8(OutPath.wstring());
                        }
                    }
                }
            }
        }
    }

    TArray<FString> MaterialAssetPaths;
    MaterialAssetPaths.reserve(Mesh.SubMeshes.size());
    for (const FSubMesh& Sub : Mesh.SubMeshes)
    {
        auto It = MaterialAssetMap.find(Sub.DefaultMaterialName);
        if (It != MaterialAssetMap.end())
        {
            MaterialAssetPaths.push_back(It->second);
        }
        else
        {
            MaterialAssetPaths.push_back("");
        }
    }

    const bool bWrote = WriteStaticMeshUAsset(Source, Mesh, Vertices, MaterialAssetPaths);
    if (bWrote && AssetManager)
    {
        FAssetLoadParams Params;
        Params.ExplicitType = EAssetType::StaticMesh;
        AssetManager->Load(MakeMeshUAssetPath(Source.NormalizedPath).native(), Params);
    }
    return bWrote;
}

bool FEditorAssetImporter::ImportMtl(const FWString& MtlPath)
{
    FSourceData Source;
    if (!LoadSource(MtlPath, Source))
    {
        return false;
    }

    TMap<FString, FMaterial> Materials;
    if (!ParseMtlText(Source, Materials))
    {
        return false;
    }

    bool bAnyWritten = false;
    const fs::path MtlFilePath(Source.NormalizedPath);

    for (const auto& Pair : Materials)
    {
        const FString& MaterialName = Pair.first;
        const FMaterial& MaterialData = Pair.second;
        const fs::path OutPath = MakeMaterialUAssetPath(MtlFilePath, MaterialName);
        if (WriteMaterialUAsset(Source, MaterialName, MaterialData, OutPath))
        {
            bAnyWritten = true;
            if (AssetManager)
            {
                FAssetLoadParams Params;
                Params.ExplicitType = EAssetType::Material;
                AssetManager->Load(OutPath.native(), Params);
            }
        }
    }

    return bAnyWritten;
}

bool FEditorAssetImporter::LoadSource(const FWString& Path, FSourceData& OutSource) const
{
    if (Path.empty())
    {
        return false;
    }

    FWString Normalized = Path;
    NormalizePathSeparators(Normalized);

    const fs::path  FilePath(Normalized);
    std::error_code ErrorCode;

    if (!fs::exists(FilePath, ErrorCode) || !fs::is_regular_file(FilePath, ErrorCode))
    {
        return false;
    }

    const uint64 FileSize = fs::file_size(FilePath, ErrorCode);
    if (ErrorCode)
    {
        return false;
    }

    std::ifstream File(FilePath, std::ios::binary);
    if (!File.is_open())
    {
        return false;
    }

    OutSource = {};
    OutSource.NormalizedPath = Normalized;
    OutSource.Bytes.resize(FileSize);

    if (FileSize > 0)
    {
        File.read(reinterpret_cast<char*>(OutSource.Bytes.data()),
                  static_cast<std::streamsize>(FileSize));
        if (!File)
        {
            return false;
        }
    }

    OutSource.SourceHash = ComputeFnv1a64(OutSource.Bytes.data(), OutSource.Bytes.size());
    return true;
}

uint64 FEditorAssetImporter::ComputeFnv1a64(const uint8* Data, size_t Size) const
{
    uint64 Hash = 14695981039346656037ull;
    for (size_t i = 0; i < Size; ++i)
    {
        Hash ^= static_cast<uint64>(Data[i]);
        Hash *= 1099511628211ull;
    }
    return Hash;
}

FString FEditorAssetImporter::WidePathToUtf8(const FWString& Path) const
{
    const std::filesystem::path FilePath(Path);
    const std::u8string         Utf8Path = FilePath.u8string();
    return FString(reinterpret_cast<const char*>(Utf8Path.data()), Utf8Path.size());
}

FWString FEditorAssetImporter::NarrowPathToWidePath(const FString& NarrowPath) const
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

bool FEditorAssetImporter::IsAbsoluteWidePath(const FWString& Path) const
{
    if (Path.size() >= 2 && Path[1] == L':')
    {
        return true;
    }

    return Path.size() >= 2 && Path[0] == L'\\' && Path[1] == L'\\';
}

void FEditorAssetImporter::NormalizePathSeparators(FWString& Path) const
{
    std::replace(Path.begin(), Path.end(), L'/', L'\\');
}

FWString FEditorAssetImporter::ResolveSiblingPath(const FWString& BaseFilePath,
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

FString FEditorAssetImporter::SanitizeFileName(const FString& Name) const
{
    FString Result = Name;
    for (char& Ch : Result)
    {
        if (Ch == '<' || Ch == '>' || Ch == ':' || Ch == '"' || Ch == '/' || Ch == '\\' ||
            Ch == '|' || Ch == '?' || Ch == '*')
        {
            Ch = '_';
        }
    }
    if (Result.empty())
    {
        Result = "Material";
    }
    return Result;
}

std::filesystem::path FEditorAssetImporter::MakeMeshUAssetPath(const FWString& ObjPath) const
{
    fs::path OutPath(ObjPath);
    OutPath.replace_extension(L".uasset");
    return OutPath;
}

std::filesystem::path FEditorAssetImporter::MakeMaterialUAssetPath(
    const std::filesystem::path& MtlPath, const FString& MaterialName) const
{
    fs::path OutPath = MtlPath.parent_path();
    const FString SafeName = SanitizeFileName(MaterialName);
    FWString WideSafeName = NarrowPathToWidePath(SafeName + ".uasset");
    if (WideSafeName.empty())
    {
        WideSafeName = L"Material.uasset";
    }
    OutPath /= fs::path(WideSafeName);
    return OutPath;
}

bool FEditorAssetImporter::ParseObjText(const FSourceData& Source, FStaticMesh& OutMesh,
                                        TArray<FMeshVertexPNCT>& OutVertices) const
{
    if (Source.Bytes.empty())
        return false;

    std::string_view FileView(reinterpret_cast<const char*>(Source.Bytes.data()),
                              Source.Bytes.size());

    TArray<FVector>  TempPositions;
    TArray<FVector2> TempUVs;
    TArray<FVector>  TempNormals;

    TMap<std::string_view, uint32> VertexCache;

    FSubMesh CurrentSubMesh;
    bool     bSubMeshActive = false;

    auto GetNextToken = [](std::string_view& View) -> std::string_view
    {
        size_t First = View.find_first_not_of(" \t\r");
        if (First == std::string_view::npos)
            return {};
        View.remove_prefix(First);

        size_t           Last = View.find_first_of(" \t\r");
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

    auto ParseInt = [](std::string_view Token, int& OutVal)
    {
        if (!Token.empty())
            std::from_chars(Token.data(), Token.data() + Token.size(), OutVal);
    };

    auto TrimView = [](std::string_view View) -> std::string_view
    {
        const size_t First = View.find_first_not_of(" \t\r");
        if (First == std::string_view::npos)
        {
            return {};
        }
        const size_t Last = View.find_last_not_of(" \t\r");
        return View.substr(First, Last - First + 1);
    };

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

        if (Header == "v")
        {
            FVector Pos = {0.f, 0.f, 0.f};
            ParseFloat(GetNextToken(LineView), Pos.X);
            ParseFloat(GetNextToken(LineView), Pos.Y);
            ParseFloat(GetNextToken(LineView), Pos.Z);
            TempPositions.push_back(Pos);

            OutMesh.BoundingBox.Min.X = std::min(OutMesh.BoundingBox.Min.X, Pos.X);
            OutMesh.BoundingBox.Min.Y = std::min(OutMesh.BoundingBox.Min.Y, Pos.Y);
            OutMesh.BoundingBox.Min.Z = std::min(OutMesh.BoundingBox.Min.Z, Pos.Z);
            OutMesh.BoundingBox.Max.X = std::max(OutMesh.BoundingBox.Max.X, Pos.X);
            OutMesh.BoundingBox.Max.Y = std::max(OutMesh.BoundingBox.Max.Y, Pos.Y);
            OutMesh.BoundingBox.Max.Z = std::max(OutMesh.BoundingBox.Max.Z, Pos.Z);
        }
        else if (Header == "vt")
        {
            FVector2 UV = {0.f, 0.f};
            ParseFloat(GetNextToken(LineView), UV.X);
            ParseFloat(GetNextToken(LineView), UV.Y);
            UV.Y = 1.0f - UV.Y;
            TempUVs.push_back(UV);
        }
        else if (Header == "vn")
        {
            FVector Normal = {0.f, 0.f, 0.f};
            ParseFloat(GetNextToken(LineView), Normal.X);
            ParseFloat(GetNextToken(LineView), Normal.Y);
            ParseFloat(GetNextToken(LineView), Normal.Z);
            TempNormals.push_back(Normal);
        }
        else if (Header == "mtllib")
        {
            std::string_view MtlFile = TrimView(LineView);

            if (!MtlFile.empty() && OutMesh.MaterialLibraryPath.empty())
            {
                OutMesh.MaterialLibraryPath = FString(MtlFile);
            }
        }
        else if (Header == "usemtl")
        {
            if (bSubMeshActive)
            {
                CurrentSubMesh.IndexCount = static_cast<uint32>(OutMesh.CPU_Indices.size()) -
                                            CurrentSubMesh.StartIndexLocation;
                if (CurrentSubMesh.IndexCount > 0)
                    OutMesh.SubMeshes.push_back(CurrentSubMesh);
            }

            std::string_view MatName = GetNextToken(LineView);
            CurrentSubMesh.DefaultMaterialName = FString(MatName);
            CurrentSubMesh.StartIndexLocation = static_cast<uint32>(OutMesh.CPU_Indices.size());
            CurrentSubMesh.IndexCount = 0;
            bSubMeshActive = true;
        }
        else if (Header == "f")
        {
            TArray<uint32> FaceIndices;

            while (true)
            {
                std::string_view VertexToken = GetNextToken(LineView);
                if (VertexToken.empty())
                    break;

                auto It = VertexCache.find(VertexToken);
                if (It != VertexCache.end())
                {
                    FaceIndices.push_back(It->second);
                }
                else
                {
                    int              V = 0, VT = 0, VN = 0;
                    std::string_view Remainder = VertexToken;

                    auto ParseFaceIndex = [&](int& OutIdx)
                    {
                        size_t           SlashPos = Remainder.find('/');
                        std::string_view SubToken = Remainder.substr(0, SlashPos);
                        ParseInt(SubToken, OutIdx);
                        if (SlashPos != std::string_view::npos)
                            Remainder.remove_prefix(SlashPos + 1);
                        else
                            Remainder = {};
                    };

                    ParseFaceIndex(V);
                    ParseFaceIndex(VT);
                    ParseFaceIndex(VN);

                    FMeshVertexPNCT NewVertex = {};
                    if (V > 0)
                        NewVertex.Position = TempPositions[V - 1];
                    if (VT > 0)
                        NewVertex.UV = TempUVs[VT - 1];
                    if (VN > 0)
                        NewVertex.Normal = TempNormals[VN - 1];

                    uint32 NewIndex = static_cast<uint32>(OutVertices.size());
                    OutVertices.push_back(NewVertex);
                    OutMesh.CPU_Positions.push_back(NewVertex.Position);

                    VertexCache[VertexToken] = NewIndex;
                    FaceIndices.push_back(NewIndex);
                }
            }

            for (size_t i = 1; i + 1 < FaceIndices.size(); ++i)
            {
                OutMesh.CPU_Indices.push_back(FaceIndices[0]);
                OutMesh.CPU_Indices.push_back(FaceIndices[i]);
                OutMesh.CPU_Indices.push_back(FaceIndices[i + 1]);
            }
        }
    }

    if (bSubMeshActive)
    {
        CurrentSubMesh.IndexCount =
            static_cast<uint32>(OutMesh.CPU_Indices.size()) - CurrentSubMesh.StartIndexLocation;
        if (CurrentSubMesh.IndexCount > 0)
            OutMesh.SubMeshes.push_back(CurrentSubMesh);
    }
    else if (!OutMesh.CPU_Indices.empty())
    {
        CurrentSubMesh.DefaultMaterialName = "DefaultMaterial";
        CurrentSubMesh.StartIndexLocation = 0;
        CurrentSubMesh.IndexCount = static_cast<uint32>(OutMesh.CPU_Indices.size());
        OutMesh.SubMeshes.push_back(CurrentSubMesh);
    }

    OutMesh.VertexCount = static_cast<uint32>(OutVertices.size());
    OutMesh.IndexCount = static_cast<uint32>(OutMesh.CPU_Indices.size());
    OutMesh.VertexStride = sizeof(FMeshVertexPNCT);

    return OutMesh.VertexCount > 0 && OutMesh.IndexCount > 0;
}

bool FEditorAssetImporter::ParseMtlText(const FSourceData& Source,
                                        TMap<FString, FMaterial>& OutMaterials) const
{
    if (Source.Bytes.empty())
        return false;

    std::string_view FileView(reinterpret_cast<const char*>(Source.Bytes.data()),
                              Source.Bytes.size());

    FString   CurrentMaterialName = "";
    FMaterial CurrentData;
    bool      bHasMaterial = false;

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
                OutMaterials[CurrentMaterialName] = CurrentData;
            }

            std::string_view MatName = GetNextToken(LineView);
            if (MatName.empty())
            {
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

    if (bHasMaterial && !CurrentMaterialName.empty())
    {
        OutMaterials[CurrentMaterialName] = CurrentData;
    }

    return !OutMaterials.empty();
}

bool FEditorAssetImporter::WriteStaticMeshUAsset(const FSourceData& Source, const FStaticMesh& Mesh,
                                                 const TArray<FMeshVertexPNCT>& Vertices,
                                                 const TArray<FString>& MaterialAssetPaths) const
{
    const fs::path OutPath = MakeMeshUAssetPath(Source.NormalizedPath);

    FWindowsBinWriter Ar(OutPath);
    if (!Ar.IsValid())
    {
        return false;
    }

    FUAssetBinaryHeader Header;
    Header.Magic = kUAssetMagic;
    Header.Version = kUAssetVersion;
    Header.AssetType = EUAssetBinaryType::StaticMesh;
    Header.SourceHash = Source.SourceHash;

    Ar << Header.Magic;
    Ar << Header.Version;
    uint32 TypeValue = static_cast<uint32>(Header.AssetType);
    Ar << TypeValue;
    Ar << Header.SourceHash;

    Ar << Mesh.VertexCount;
    Ar << Mesh.IndexCount;
    Ar << Mesh.VertexStride;

    Ar.Serialize(const_cast<FVector*>(&Mesh.BoundingBox.Min), sizeof(FVector));
    Ar.Serialize(const_cast<FVector*>(&Mesh.BoundingBox.Max), sizeof(FVector));

    FString MaterialLibraryPath = Mesh.MaterialLibraryPath;
    Ar << MaterialLibraryPath;

    uint32 SubMeshCount = static_cast<uint32>(Mesh.SubMeshes.size());
    Ar << SubMeshCount;
    for (const FSubMesh& Sub : Mesh.SubMeshes)
    {
        FString MatName = Sub.DefaultMaterialName;
        Ar << MatName;
        Ar << Sub.StartIndexLocation;
        Ar << Sub.IndexCount;
    }

    uint32 MaterialPathCount = static_cast<uint32>(MaterialAssetPaths.size());
    Ar << MaterialPathCount;
    for (const FString& Path : MaterialAssetPaths)
    {
        FString PathCopy = Path;
        Ar << PathCopy;
    }

    TArray<FVector> PositionsCopy = Mesh.CPU_Positions;
    TArray<uint32> IndicesCopy = Mesh.CPU_Indices;
    TArray<FMeshVertexPNCT> VerticesCopy = Vertices;

    SerializeArrayRaw(Ar, PositionsCopy);
    SerializeArray(Ar, IndicesCopy);
    SerializeArrayRaw(Ar, VerticesCopy);

    return !Ar.HasError();
}

bool FEditorAssetImporter::WriteMaterialUAsset(const FSourceData& Source,
                                               const FString& MaterialName,
                                               const FMaterial& MaterialData,
                                               const std::filesystem::path& OutPath) const
{
    FWindowsBinWriter Ar(OutPath);
    if (!Ar.IsValid())
    {
        return false;
    }

    FUAssetBinaryHeader Header;
    Header.Magic = kUAssetMagic;
    Header.Version = kUAssetVersion;
    Header.AssetType = EUAssetBinaryType::Material;
    Header.SourceHash = Source.SourceHash;

    Ar << Header.Magic;
    Ar << Header.Version;
    uint32 TypeValue = static_cast<uint32>(Header.AssetType);
    Ar << TypeValue;
    Ar << Header.SourceHash;

    FString NameCopy = MaterialName;
    Ar << NameCopy;

    Ar.Serialize(const_cast<FVector*>(&MaterialData.AmbientColor), sizeof(FVector));
    Ar.Serialize(const_cast<FVector*>(&MaterialData.DiffuseColor), sizeof(FVector));
    Ar.Serialize(const_cast<FVector*>(&MaterialData.SpecularColor), sizeof(FVector));
    Ar.Serialize(const_cast<FVector*>(&MaterialData.EmissiveColor), sizeof(FVector));
    Ar.Serialize(const_cast<FVector*>(&MaterialData.TransmissionFilter), sizeof(FVector));

    float SpecularHighlight = MaterialData.SpecularHighlight;
    float OpticalDensity = MaterialData.OpticalDensity;
    float Opacity = MaterialData.Opacity;
    int32 IlluminationModel = MaterialData.IlluminationModel;

    Ar << SpecularHighlight;
    Ar << OpticalDensity;
    Ar << Opacity;
    Ar << IlluminationModel;

    FString AmbientMapPath = MaterialData.AmbientMapPath;
    FString DiffuseMapPath = MaterialData.DiffuseMapPath;
    FString SpecularMapPath = MaterialData.SpecularHighlightMapPath;
    FString NormalMapPath = MaterialData.NormalMapPath;

    Ar << AmbientMapPath;
    Ar << DiffuseMapPath;
    Ar << SpecularMapPath;
    Ar << NormalMapPath;

    Ar.Serialize(const_cast<FVector2*>(&MaterialData.UVScrollSpeed), sizeof(FVector2));

    return !Ar.HasError();
}
