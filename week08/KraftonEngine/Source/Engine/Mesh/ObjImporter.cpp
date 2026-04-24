// 메시 영역의 세부 동작을 구현합니다.
#include "Mesh/ObjImporter.h"
#include "Mesh/StaticMeshAsset.h"
#include "Materials/Material.h"
#include "Editor/UI/EditorConsolePanel.h"
#include "Engine/Platform/Paths.h"
#include "Mesh/ObjManager.h"
#include "SimpleJSON/json.hpp"
#include "Materials/MaterialManager.h"
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <charconv>
#include <chrono>

const FVector FallbackColor3 = FVector(1.0f, 0.0f, 1.0f);
const FVector4 FallbackColor4 = FVector4(1.0f, 0.0f, 1.0f, 1.0f);

// FVertexKey는 메시 처리에 필요한 데이터를 묶는 구조체입니다.
struct FVertexKey
{
    uint32 p, t, n;
    bool operator==(const FVertexKey& Other) const
    {
        return p == Other.p && t == Other.t && n == Other.n;
    }
};

namespace std
{
template <>
// hash는 메시 처리에 필요한 데이터를 묶는 구조체입니다.
struct hash<FVertexKey>
{
    size_t operator()(const FVertexKey& Key) const noexcept
    {
        return ((size_t)Key.p) ^ (((size_t)Key.t) << 8) ^ (((size_t)Key.n) << 16);
    }
};
} // namespace std

// FStringParser는 메시 처리에 필요한 데이터를 묶는 구조체입니다.
struct FStringParser
{
    static std::string_view GetNextToken(std::string_view& InOutView, char Delimiter = ' ')
    {
        size_t DelimiterPosition = InOutView.find(Delimiter);
        std::string_view Token = InOutView.substr(0, DelimiterPosition);
        if (DelimiterPosition != std::string_view::npos)
        {
            InOutView.remove_prefix(DelimiterPosition + 1);
        }
        else
        {
            InOutView = std::string_view();
        }
        return Token;
    }

    static std::string_view GetNextWhitespaceToken(std::string_view& InOutView)
    {
        size_t Start = InOutView.find_first_not_of(" \t");
        if (Start == std::string_view::npos)
        {
            InOutView = std::string_view();
            return std::string_view();
        }
        InOutView.remove_prefix(Start);

        size_t End = InOutView.find_first_of(" \t");
        std::string_view Token = InOutView.substr(0, End);

        if (End != std::string_view::npos)
        {
            InOutView.remove_prefix(End);
        }
        else
        {
            InOutView = std::string_view();
        }
        return Token;
    }

    static void TrimLeft(std::string_view& InOutView)
    {
        size_t Start = InOutView.find_first_not_of(" \t");
        if (Start != std::string_view::npos)
        {
            InOutView.remove_prefix(Start);
        }
        else
        {
            InOutView = std::string_view();
        }
    }

    static bool ParseInt(std::string_view Str, int& OutValue)
    {
        if (Str.empty())
            return false;
        std::from_chars_result result = std::from_chars(Str.data(), Str.data() + Str.size(), OutValue);
        return result.ec == std::errc();
    }

    static bool ParseFloat(std::string_view Str, float& OutValue)
    {
        if (Str.empty())
            return false;
        std::from_chars_result result = std::from_chars(Str.data(), Str.data() + Str.size(), OutValue);
        return result.ec == std::errc();
    }
};

static FString ExtractMtlTexturePath(std::string_view Line)
{
    std::string TextureFileName;

    while (!Line.empty())
    {
        std::string_view LineBeforeToken = Line;
        std::string_view Token = FStringParser::GetNextWhitespaceToken(Line);

        if (Token.empty())
            break;

        if (Token[0] == '-')
        {
            int32 ArgsToSkip = 0;
            if (Token == "-s" || Token == "-o" || Token == "-t")
            {
                ArgsToSkip = 3;
            }
            else if (Token == "-mm")
            {
                ArgsToSkip = 2;
            }
            else if (Token == "-bm" || Token == "-boost" || Token == "-texres" ||
                     Token == "-blendu" || Token == "-blendv" || Token == "-clamp" ||
                     Token == "-cc" || Token == "-imfchan" || Token == "-type")
            {
                ArgsToSkip = 1;
            }

            for (int32 i = 0; i < ArgsToSkip; ++i)
            {
                FStringParser::GetNextWhitespaceToken(Line);
            }
        }
        else
        {
            FStringParser::TrimLeft(LineBeforeToken);
            TextureFileName = FString(LineBeforeToken);
            break;
        }
    }

    const size_t LastNonSpace = TextureFileName.find_last_not_of(" \t");
    if (LastNonSpace != FString::npos)
    {
        TextureFileName.erase(LastNonSpace + 1);
    }

    return TextureFileName;
}

// FRawFaceVertex는 메시 처리에 필요한 데이터를 묶는 구조체입니다.
struct FRawFaceVertex
{
    int32 PosIndex = -1;
    int32 UVIndex = -1;
    int32 NormalIndex = -1;
};

FRawFaceVertex ParseSingleFaceVertex(std::string_view FaceToken)
{
    FRawFaceVertex Result;

    std::string_view PosStr = FStringParser::GetNextToken(FaceToken, '/');
    FStringParser::ParseInt(PosStr, Result.PosIndex);

    if (!FaceToken.empty())
    {
        std::string_view UVStr = FStringParser::GetNextToken(FaceToken, '/');
        if (!UVStr.empty())
        {
            FStringParser::ParseInt(UVStr, Result.UVIndex);
        }
    }

    if (!FaceToken.empty())
    {
        std::string_view NormalStr = FStringParser::GetNextToken(FaceToken, '/');
        FStringParser::ParseInt(NormalStr, Result.NormalIndex);
    }

    return Result;
}

bool FObjImporter::ParseObj(const FString& ObjFilePath, FObjInfo& OutObjInfo)
{
    OutObjInfo = FObjInfo();

    std::ifstream File(FPaths::ToWide(ObjFilePath), std::ios::binary | std::ios::ate);
    if (!File.is_open())
    {
        UE_LOG("Failed to open OBJ file: %s", ObjFilePath.c_str());
        return false;
    }

    size_t FileSize = static_cast<size_t>(File.tellg());
    File.seekg(0, std::ios::beg);
    TArray<char> Buffer(FileSize);
    if (!File.read(Buffer.data(), FileSize))
    {
        UE_LOG("Failed to read OBJ file: %s", ObjFilePath.c_str());
        return false;
    }

    std::string_view FileView(Buffer.data(), Buffer.size());

    if (FileView.size() >= 3 && FileView[0] == '\xEF' && FileView[1] == '\xBB' && FileView[2] == '\xBF')
    {
        FileView.remove_prefix(3);
    }

    TArray<FRawFaceVertex> FaceVertices;
    FaceVertices.reserve(6); // Heuristic

    while (!FileView.empty())
    {
        std::string_view Line = FStringParser::GetNextToken(FileView, '\n');

        if (!Line.empty() && Line.back() == '\r')
        {
            Line.remove_suffix(1);
        }

        if (Line.empty() || Line[0] == '#')
        {
            continue;
        }

        std::string_view Prefix = FStringParser::GetNextToken(Line);

        if (Prefix == "v")
        {
            FVector Position;
            FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Position.X);
            FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Position.Y);
            FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Position.Z);
            OutObjInfo.Positions.emplace_back(Position);
        }
        else if (Prefix == "vt")
        {
            FVector2 UV;
            FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), UV.U);
            FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), UV.V);
            OutObjInfo.UVs.emplace_back(UV);
        }
        else if (Prefix == "vn")
        {
            FVector Normal;
            FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Normal.X);
            FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Normal.Y);
            FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Normal.Z);
            OutObjInfo.Normals.emplace_back(Normal);
        }
        else if (Prefix == "f")
        {
            if (OutObjInfo.Sections.empty())
            {
                FStaticMeshSection DefaultSection;
                DefaultSection.MaterialSlotName = "None";
                DefaultSection.FirstIndex = 0;
                DefaultSection.NumTriangles = 0;
                OutObjInfo.Sections.emplace_back(DefaultSection);
            }

            while (!Line.empty())
            {
                std::string_view FaceToken = FStringParser::GetNextToken(Line, ' ');
                if (!FaceToken.empty())
                {
                    FaceVertices.push_back(ParseSingleFaceVertex(FaceToken));
                }
            }

            if (FaceVertices.size() < 3)
            {
                UE_LOG("Face with less than 3 vertices");
                continue;
            }

            // Fan triangulation
            for (size_t i = 1; i + 1 < FaceVertices.size(); ++i)
            {
                const std::array<FRawFaceVertex, 3> TriangleVerts = { FaceVertices[0], FaceVertices[i], FaceVertices[i + 1] };
                for (int j = 0; j < 3; ++j)
                {
                    constexpr int32 InvalidIndex = -1;
                    OutObjInfo.PosIndices.emplace_back(TriangleVerts[j].PosIndex - 1);
                    OutObjInfo.UVIndices.emplace_back(TriangleVerts[j].UVIndex > 0 ? TriangleVerts[j].UVIndex - 1 : InvalidIndex);
                    OutObjInfo.NormalIndices.emplace_back(TriangleVerts[j].NormalIndex > 0 ? TriangleVerts[j].NormalIndex - 1 : InvalidIndex);
                }
            }
            FaceVertices.clear();
        }
        else
        {
            if (Prefix == "mtllib")
            {
                size_t CommentPos = Line.find('#');
                if (CommentPos != std::string_view::npos)
                {
                    Line = Line.substr(0, CommentPos);
                }
                FStringParser::TrimLeft(Line);
                OutObjInfo.MaterialLibraryFilePath = FPaths::ResolveAssetPath(ObjFilePath, std::string(Line));
                UE_LOG("Found material library: %s", OutObjInfo.MaterialLibraryFilePath.c_str());
            }
            else if (Prefix == "usemtl")
            {
                size_t CommentPos = Line.find('#');
                if (CommentPos != std::string_view::npos)
                {
                    Line = Line.substr(0, CommentPos);
                }
                FStringParser::TrimLeft(Line);

                if (!OutObjInfo.Sections.empty())
                {
                    OutObjInfo.Sections.back().NumTriangles = (static_cast<uint32>(OutObjInfo.PosIndices.size()) - OutObjInfo.Sections.back().FirstIndex) / 3;
                }
                FStaticMeshSection Section;
                Section.MaterialSlotName = std::string(Line);
                if (Section.MaterialSlotName.empty())
                {
                    Section.MaterialSlotName = "None";
                }
                Section.FirstIndex = static_cast<uint32>(OutObjInfo.PosIndices.size());
                OutObjInfo.Sections.emplace_back(Section);
            }
            else if (Prefix == "o")
            {
                size_t CommentPos = Line.find('#');
                if (CommentPos != std::string_view::npos)
                {
                    Line = Line.substr(0, CommentPos);
                }
                FStringParser::TrimLeft(Line);

                OutObjInfo.ObjectName = std::string(Line);
            }
        }
    }

    if (!OutObjInfo.Sections.empty())
    {
        OutObjInfo.Sections.back().NumTriangles = (static_cast<uint32>(OutObjInfo.PosIndices.size()) - OutObjInfo.Sections.back().FirstIndex) / 3;
    }

    if (OutObjInfo.UVs.empty())
    {
        OutObjInfo.UVs.emplace_back(FVector2{ 0.0f, 0.0f });
    }

    return true;
}

bool FObjImporter::ParseMtl(const FString& MtlFilePath, TArray<FObjMaterialInfo>& OutMtlInfos)
{
    OutMtlInfos.clear();
    std::ifstream File(FPaths::ToWide(MtlFilePath), std::ios::binary | std::ios::ate);

    if (!File.is_open())
    {
        UE_LOG("Failed to open MTL file: %s", MtlFilePath.c_str());
        return false;
    }

    size_t FileSize = static_cast<size_t>(File.tellg());
    File.seekg(0, std::ios::beg);
    TArray<char> Buffer(FileSize);
    if (!File.read(Buffer.data(), FileSize))
    {
        UE_LOG("Failed to read MTL file: %s", MtlFilePath.c_str());
        return false;
    }

    std::string_view FileView(Buffer.data(), Buffer.size());

    if (FileView.size() >= 3 && FileView[0] == '\xEF' && FileView[1] == '\xBB' && FileView[2] == '\xBF')
    {
        FileView.remove_prefix(3);
    }

    while (!FileView.empty())
    {
        std::string_view Line = FStringParser::GetNextToken(FileView, '\n');

        if (!Line.empty() && Line.back() == '\r')
        {
            Line.remove_suffix(1);
        }

        if (Line.empty() || Line[0] == '#')
        {
            continue;
        }

        std::string_view Prefix = FStringParser::GetNextWhitespaceToken(Line);

        if (Prefix == "newmtl")
        {
            FObjMaterialInfo MaterialInfo;
            FStringParser::TrimLeft(Line);
            MaterialInfo.MaterialSlotName = std::string(Line);
            OutMtlInfos.emplace_back(MaterialInfo);
        }
        else if (Prefix == "Ka")
        {
            if (OutMtlInfos.empty())
            {
                continue;
            }
            FObjMaterialInfo& CurrentMaterial = OutMtlInfos.back();
            FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Ka.X);
            FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Ka.Y);
            FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Ka.Z);
        }
        else if (Prefix == "Kd")
        {
            if (OutMtlInfos.empty())
            {
                continue;
            }
            FObjMaterialInfo& CurrentMaterial = OutMtlInfos.back();
            FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Kd.X);
            FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Kd.Y);
            FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Kd.Z);
        }
        else if (Prefix == "Ks")
        {
            if (OutMtlInfos.empty())
            {
                continue;
            }
            FObjMaterialInfo& CurrentMaterial = OutMtlInfos.back();
            FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Ks.X);
            FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Ks.Y);
            FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Ks.Z);
        }
        else if (Prefix == "Ns")
        {
            if (!OutMtlInfos.empty())
            {
                FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), OutMtlInfos.back().Ns);
            }
        }
        else if (Prefix == "Ni")
        {
            if (!OutMtlInfos.empty())
            {
                FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), OutMtlInfos.back().Ni);
            }
        }
        else if (Prefix == "illum")
        {
            if (!OutMtlInfos.empty())
            {
                FStringParser::ParseInt(FStringParser::GetNextWhitespaceToken(Line), OutMtlInfos.back().illum);
            }
        }
        else if (Prefix == "map_Kd" || Prefix == "map_Ks" || Prefix == "map_Bump" || Prefix == "map_bump" || Prefix == "bump" || Prefix == "norm")
        {
            if (OutMtlInfos.empty())
            {
                continue;
            }

            const FString TextureFileName = ExtractMtlTexturePath(Line);
            if (!TextureFileName.empty())
            {
                const FString ResolvedPath = FPaths::ResolveAssetPath(MtlFilePath, TextureFileName);
                if (Prefix == "map_Kd")
                {
                    OutMtlInfos.back().map_Kd = ResolvedPath;
                }
                else if (Prefix == "map_Ks")
                {
                    OutMtlInfos.back().map_Ks = ResolvedPath;
                }
                else
                {
                    OutMtlInfos.back().map_Bump = ResolvedPath;
                }
            }
        }
    }

    return true;
}

FString FObjImporter::ConvertMtlInfoToJson(const FObjMaterialInfo* MtlInfo)
{
    std::filesystem::path JsonFullPath;
    auto TryBuildMaterialPathFromAsset = [&](const FString& AssetPath)
    {
        if (AssetPath.empty() || !JsonFullPath.empty())
        {
            return;
        }
        std::filesystem::path AssetRoot = FPaths::ToPath(FPaths::AssetDir()).lexically_normal();
        std::filesystem::path FullAssetPath = (FPaths::ToPath(FPaths::RootDir()) / FPaths::ToPath(FPaths::ToWide(AssetPath))).lexically_normal();
        std::filesystem::path Relative = FullAssetPath.lexically_relative(AssetRoot);
        if (Relative.empty())
        {
            return;
        }
        std::filesystem::path Rebased;
        bool bReplaced = false;
        for (const auto& Part : Relative)
        {
            std::wstring Component = Part.wstring();
            std::transform(Component.begin(), Component.end(), Component.begin(), ::towlower);
            if (!bReplaced && Component == L"models")
            {
                Rebased /= L"Materials";
                bReplaced = true;
            }
            else
            {
                Rebased /= Part;
            }
        }
        if (bReplaced)
        {
            JsonFullPath = (AssetRoot / Rebased.parent_path() / (FPaths::ToWide(MtlInfo->MaterialSlotName) + L".json")).lexically_normal();
        }
    };
    TryBuildMaterialPathFromAsset(MtlInfo->map_Kd);
    TryBuildMaterialPathFromAsset(MtlInfo->map_Ks);
    TryBuildMaterialPathFromAsset(MtlInfo->map_Bump);
    if (JsonFullPath.empty())
    {
        JsonFullPath = (FPaths::ToPath(FPaths::ContentDir()) / L"Materials" / (FPaths::ToWide(MtlInfo->MaterialSlotName) + L".json")).lexically_normal();
    }
    FPaths::CreateDir(JsonFullPath.parent_path().wstring());
    const FString JsonPath = FPaths::MakeRelativeToRoot(JsonFullPath);
    if (std::filesystem::exists(JsonFullPath))
        return JsonPath;

    json::JSON JsonData;
    JsonData["PathFileName"] = JsonPath;
    JsonData["BlendState"] = "Opaque";
    JsonData["DepthStencilState"] = "Default";
    JsonData["RasterizerState"] = "SolidBackCull";

    if (!MtlInfo->map_Kd.empty())
    {
        JsonData["Textures"]["DiffuseTexture"] = MtlInfo->map_Kd;

        JsonData["Parameters"]["SectionColor"][0] = 1.0f;
        JsonData["Parameters"]["SectionColor"][1] = 1.0f;
        JsonData["Parameters"]["SectionColor"][2] = 1.0f;
        JsonData["Parameters"]["SectionColor"][3] = 1.0f;
    }
    else
    {
        JsonData["Parameters"]["SectionColor"][0] = MtlInfo->Kd.X;
        JsonData["Parameters"]["SectionColor"][1] = MtlInfo->Kd.Y;
        JsonData["Parameters"]["SectionColor"][2] = MtlInfo->Kd.Z;
        JsonData["Parameters"]["SectionColor"][3] = 1.0f;
    }

    if (!MtlInfo->map_Bump.empty())
    {
        JsonData["Textures"]["NormalTexture"] = MtlInfo->map_Bump;
    }

    if (!MtlInfo->map_Ks.empty())
    {
        JsonData["Textures"]["SpecularTexture"] = MtlInfo->map_Ks;
    }

    JsonData["Parameters"]["SpecularPower"] = (MtlInfo->Ns > 0.0f) ? MtlInfo->Ns : 32.0f;
    const float AvgSpecular = (MtlInfo->Ks.X + MtlInfo->Ks.Y + MtlInfo->Ks.Z) / 3.0f;
    JsonData["Parameters"]["SpecularStrength"] = (AvgSpecular > 0.0f) ? AvgSpecular : 1.0f;

    std::ofstream File(FPaths::ToWide(JsonPath));
    File << JsonData.dump();

    return JsonPath;
}

FVector FObjImporter::RemapPosition(const FVector& ObjPos, EForwardAxis Axis)
{
    switch (Axis)
    {
    case EForwardAxis::X:
        return FVector(ObjPos.X, ObjPos.Z, ObjPos.Y);
    case EForwardAxis::NegX:
        return FVector(-ObjPos.X, -ObjPos.Z, ObjPos.Y);
    case EForwardAxis::Y:
        return FVector(ObjPos.Y, ObjPos.X, ObjPos.Z);
    case EForwardAxis::NegY:
        return FVector(-ObjPos.Y, -ObjPos.X, ObjPos.Z);
    case EForwardAxis::Z:
        return FVector(ObjPos.Z, ObjPos.X, ObjPos.Y);
    case EForwardAxis::NegZ:
        return FVector(-ObjPos.Z, ObjPos.X, ObjPos.Y);
    default:
        return FVector(ObjPos.X, ObjPos.Z, ObjPos.Y);
    }
}

bool FObjImporter::Convert(const FObjInfo& ObjInfo, const TArray<FObjMaterialInfo>& MtlInfos, const FImportOptions& Options, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
    OutMesh = FStaticMesh();
    OutMaterials.clear();

    TArray<FString> OrderedMaterialSlots;
    bool bHasNoneSlot = false;

    for (const FStaticMeshSection& Section : ObjInfo.Sections)
    {
        const FString& CurrentSlotName = Section.MaterialSlotName;

        if (CurrentSlotName == "None")
        {
            bHasNoneSlot = true;
            continue;
        }

        if (std::find(OrderedMaterialSlots.begin(), OrderedMaterialSlots.end(), CurrentSlotName) == OrderedMaterialSlots.end())
        {
            OrderedMaterialSlots.push_back(CurrentSlotName);
        }
    }

    for (const FString& TargetSlotName : OrderedMaterialSlots)
    {
        const FObjMaterialInfo* MatchedMaterial = nullptr;
        auto It = std::find_if(MtlInfos.begin(), MtlInfos.end(),
                               [&TargetSlotName](const FObjMaterialInfo& Mat)
                               {
                                   return Mat.MaterialSlotName == TargetSlotName;
                               });

        if (It != MtlInfos.end())
        {
            MatchedMaterial = &(*It);
            UE_LOG("Importer TargetSlotName: %s;", TargetSlotName.c_str());

            FString JsonPath = ConvertMtlInfoToJson(MatchedMaterial);
            UMaterial* MaterialObject = FMaterialManager::Get().GetOrCreateStaticMeshMaterial(JsonPath);

            FStaticMaterial NewStaticMaterial;
            NewStaticMaterial.MaterialInterface = MaterialObject;
            NewStaticMaterial.MaterialSlotName = TargetSlotName;
            OutMaterials.push_back(NewStaticMaterial);
        }
        else
        {
            UMaterial* DefaultMaterial = FMaterialManager::Get().GetOrCreateMaterial("None");

            FStaticMaterial NewEmptyStaticMaterial;
            NewEmptyStaticMaterial.MaterialInterface = DefaultMaterial;
            NewEmptyStaticMaterial.MaterialSlotName = TargetSlotName;
            OutMaterials.push_back(NewEmptyStaticMaterial);
        }
    }

    if (bHasNoneSlot)
    {
        UMaterial* DefaultMaterial = FMaterialManager::Get().GetOrCreateMaterial("None");

        FStaticMaterial NewDefaultStaticMaterial;
        NewDefaultStaticMaterial.MaterialInterface = DefaultMaterial;
        NewDefaultStaticMaterial.MaterialSlotName = "None";

        OutMaterials.push_back(NewDefaultStaticMaterial);
    }

    TArray<TArray<uint32>> FacesPerMaterial;
    FacesPerMaterial.resize(OutMaterials.size());

    for (const FStaticMeshSection& RawSection : ObjInfo.Sections)
    {
        auto It = std::find_if(OutMaterials.begin(), OutMaterials.end(),
                               [&RawSection](const FStaticMaterial& Mat)
                               {
                                   return Mat.MaterialSlotName == RawSection.MaterialSlotName;
                               });

        size_t MaterialIndex = 0;
        if (It != OutMaterials.end())
        {
            MaterialIndex = std::distance(OutMaterials.begin(), It);
        }
        else
        {
            MaterialIndex = OutMaterials.size() - 1;
            UE_LOG("Warning: Material slot '%s' not found. Assigning to Default slot.", RawSection.MaterialSlotName.c_str());
        }

        for (uint32 i = 0; i < RawSection.NumTriangles; ++i)
        {
            uint32 FaceStartIndex = RawSection.FirstIndex + (i * 3);
            FacesPerMaterial[MaterialIndex].push_back(FaceStartIndex);
        }
    }

    TMap<FVertexKey, uint32> VertexMap;

    for (size_t MaterialIndex = 0; MaterialIndex < OutMaterials.size(); ++MaterialIndex)
    {
        const TArray<uint32>& FaceStarts = FacesPerMaterial[MaterialIndex];
        if (FaceStarts.empty())
            continue;

        FStaticMeshSection NewSection;
        NewSection.MaterialIndex = static_cast<int32>(MaterialIndex);
        NewSection.MaterialSlotName = OutMaterials[MaterialIndex].MaterialSlotName;
        NewSection.FirstIndex = static_cast<uint32>(OutMesh.Indices.size());
        NewSection.NumTriangles = static_cast<uint32>(FaceStarts.size());

        for (uint32 FaceStartIndex : FaceStarts)
        {
            uint32 TriangleIndices[3];

            FVector P0 = ObjInfo.Positions[ObjInfo.PosIndices[FaceStartIndex]];
            FVector P1 = ObjInfo.Positions[ObjInfo.PosIndices[FaceStartIndex + 1]];
            FVector P2 = ObjInfo.Positions[ObjInfo.PosIndices[FaceStartIndex + 2]];

            FVector Edge1 = P1 - P0;
            FVector Edge2 = P2 - P0;
            FVector FaceNormal = Edge1.Cross(Edge2).Normalized();

            FVector T(1, 0, 0), B(0, 1, 0);
            if (ObjInfo.UVIndices[FaceStartIndex] != -1)
            {
                FVector2 uv0 = ObjInfo.UVs[ObjInfo.UVIndices[FaceStartIndex]];
                FVector2 uv1 = ObjInfo.UVs[ObjInfo.UVIndices[FaceStartIndex + 1]];
                FVector2 uv2 = ObjInfo.UVs[ObjInfo.UVIndices[FaceStartIndex + 2]];

                FVector2 deltaUV1 = uv1 - uv0;
                FVector2 deltaUV2 = uv2 - uv0;

                float f = 1.0f / (deltaUV1.X * deltaUV2.Y - deltaUV2.X * deltaUV1.Y);
                if (!std::isinf(f) && !std::isnan(f))
                {
                    T.X = f * (deltaUV2.Y * Edge1.X - deltaUV1.Y * Edge2.X);
                    T.Y = f * (deltaUV2.Y * Edge1.Y - deltaUV1.Y * Edge2.Y);
                    T.Z = f * (deltaUV2.Y * Edge1.Z - deltaUV1.Y * Edge2.Z);

                    B.X = f * (-deltaUV2.X * Edge1.X + deltaUV1.X * Edge2.X);
                    B.Y = f * (-deltaUV2.X * Edge1.Y + deltaUV1.X * Edge2.Y);
                    B.Z = f * (-deltaUV2.X * Edge1.Z + deltaUV1.X * Edge2.Z);
                }
            }
            T.Normalize();
            B.Normalize();

            for (int j = 0; j < 3; ++j)
            {
                size_t CurrentIndex = FaceStartIndex + j;
                FVertexKey Key = {
                    ObjInfo.PosIndices[CurrentIndex],
                    ObjInfo.UVIndices[CurrentIndex],
                    ObjInfo.NormalIndices[CurrentIndex]
                };

                if (auto It = VertexMap.find(Key); It != VertexMap.end())
                {
                    TriangleIndices[j] = It->second;
                }
                else
                {
                    FVertexPNCT_T NewVertex;

                    NewVertex.Position = RemapPosition(ObjInfo.Positions[Key.p], Options.ForwardAxis) * Options.Scale;

                    FVector FinalNormal;
                    if (Key.n == -1)
                    {
                        FinalNormal = RemapPosition(FaceNormal, Options.ForwardAxis).Normalized();
                    }
                    else
                    {
                        FinalNormal = RemapPosition(ObjInfo.Normals[Key.n], Options.ForwardAxis).Normalized();
                    }
                    NewVertex.Normal = FinalNormal;

                    if (Key.t == -1)
                    {
                        NewVertex.UV = { 0.0f, 0.0f };
                    }
                    else
                    {
                        NewVertex.UV = ObjInfo.UVs[Key.t];
                        NewVertex.UV.V = 1.0f - NewVertex.UV.V;
                    }

                    NewVertex.Color = { 1.0f, 1.0f, 1.0f, 1.0f };

                    FVector WorldT = RemapPosition(T, Options.ForwardAxis).Normalized();
                    FVector WorldB = RemapPosition(B, Options.ForwardAxis).Normalized();
                    float w = (FinalNormal.Cross(WorldT).Dot(WorldB) < 0.0f) ? -1.0f : 1.0f;
                    NewVertex.Tangent = FVector4(WorldT.X, WorldT.Y, WorldT.Z, w);

                    uint32 NewIndex = static_cast<uint32>(OutMesh.Vertices.size());
                    OutMesh.Vertices.push_back(NewVertex);

                    VertexMap[Key] = NewIndex;
                    TriangleIndices[j] = NewIndex;
                }
            }

            OutMesh.Indices.push_back(TriangleIndices[0]);
            if (Options.WindingOrder == EWindingOrder::CCW_to_CW)
            {
                OutMesh.Indices.push_back(TriangleIndices[2]);
                OutMesh.Indices.push_back(TriangleIndices[1]);
            }
            else
            {
                OutMesh.Indices.push_back(TriangleIndices[1]);
                OutMesh.Indices.push_back(TriangleIndices[2]);
            }
        }

        OutMesh.Sections.push_back(NewSection);
    }

    return true;
}

bool FObjImporter::Import(const FString& ObjFilePath, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
    return Import(ObjFilePath, FImportOptions::Default(), OutMesh, OutMaterials);
}

bool FObjImporter::Import(const FString& ObjFilePath, const FImportOptions& Options, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
    auto StartTime = std::chrono::high_resolution_clock::now();

    OutMaterials.clear();

    FObjInfo ObjInfo;
    if (!FObjImporter::ParseObj(ObjFilePath, ObjInfo))
    {
        UE_LOG("ParseObj failed for: %s", ObjFilePath.c_str());
        return false;
    }

    TArray<FObjMaterialInfo> ParsedMtlInfos;
    if (!ObjInfo.MaterialLibraryFilePath.empty())
    {
        if (!FObjImporter::ParseMtl(ObjInfo.MaterialLibraryFilePath, ParsedMtlInfos))
        {
            UE_LOG("ParseMtl failed for: %s", ObjInfo.MaterialLibraryFilePath.c_str());
            ObjInfo.MaterialLibraryFilePath.clear();
            ParsedMtlInfos.clear();
        }
    }

    if (!FObjImporter::Convert(ObjInfo, ParsedMtlInfos, Options, OutMesh, OutMaterials))
    {
        UE_LOG("Convert failed for: %s", ObjFilePath.c_str());
        return false;
    }
    OutMesh.PathFileName = ObjFilePath;

    auto EndTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> Duration = EndTime - StartTime;
    UE_LOG("OBJ Imported successfully. File: %s. Time taken: %.4f seconds", ObjFilePath.c_str(), Duration.count());

    return true;
}
