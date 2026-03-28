#include "Core/CoreMinimal.h"
#include "StaticMeshLoader.h"

#include "StaticMesh.h"
#include "Renderer/D3D11/D3D11RHI.h"

#include <filesystem>
#include <sstream>
#include <cwctype>
#include <algorithm>

namespace fs = std::filesystem;

// 임시 정점 구조체 (로더 내부에서 파싱용으로 사용)
struct FVertex
{
    FVector  Position;
    FVector2 UV;
    FVector  Normal;
};

FStaticMeshLoader::FStaticMeshLoader(FD3D11RHI* InRHI, UAssetManager* InAssetManager)
    : RHI(InRHI), AssetManager(InAssetManager)
{
}

bool FStaticMeshLoader::CanLoad(const FWString& Path, const FAssetLoadParams& Params) const
{
    if (Path.empty())
        return false;

    if (Params.ExplicitType != EAssetType::Unknown && Params.ExplicitType != EAssetType::Mesh)
    {
        return false;
    }

    const fs::path FilePath(Path);
    FWString       Extension = FilePath.extension().native();
    std::transform(Extension.begin(), Extension.end(), Extension.begin(),
                   [](wchar_t Ch) { return static_cast<wchar_t>(std::towlower(Ch)); });

    return Extension == L".obj";
}

EAssetType FStaticMeshLoader::GetAssetType() const { return EAssetType::Mesh; }

uint64 FStaticMeshLoader::MakeBuildSignature(const FAssetLoadParams& Params) const
{
    (void)Params;
    uint64 Hash = 14695981039346656037ull;
    Hash ^= static_cast<uint64>(GetAssetType());
    Hash *= 1099511628211ull;
    return Hash;
}

UAsset* FStaticMeshLoader::LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params)
{
    (void)Params;

    std::shared_ptr<FStaticMeshResource> MeshResource = std::make_shared<FStaticMeshResource>();
    MeshResource->Reset();

    TArray<FVertex> ParsedVertices;

    // 1. OBJ 텍스트 파싱 및 CPU 데이터 구성
    if (!ParseObjText(Source, *MeshResource, ParsedVertices))
    {
        UE_LOG(Asset, ELogVerbosity::Warning,
               "[StaticMeshLoader] Failed to parse .obj file. Path=%s",
               WidePathToUtf8(Source.NormalizedPath).c_str());
        return nullptr;
    }

    // 2. 파싱된 데이터를 바탕으로 GPU 버퍼(VRAM) 생성
    if (!CreateBuffers(ParsedVertices, *MeshResource))
    {
        UE_LOG(Asset, ELogVerbosity::Warning,
               "[StaticMeshLoader] Failed to create D3D11 buffers. Path=%s",
               WidePathToUtf8(Source.NormalizedPath).c_str());
        return nullptr;
    }

    // 3. UStaticMesh 에셋 생성 및 리소스 연결
    UStaticMesh* NewMeshAsset = new UStaticMesh();
    NewMeshAsset->Initialize(MeshResource);
    NewMeshAsset->SetNumSections(static_cast<uint32>(MeshResource->SubMeshes.size()));

    // .obj 파일을 읽으면서 기록해두었던 .mtl 파일들을 load 하도록 AssetManager의 Load 함수 호출
    if (AssetManager)
    {
        const fs::path ObjFilePath(Source.NormalizedPath);
        const fs::path ObjDir = ObjFilePath.parent_path();

        for (const FString& MtlFileName : MeshResource->MaterialLibraryPaths)
        {
            const fs::path MtlRelativePath(MtlFileName.c_str());
            const fs::path FullMtlPath = ObjDir / MtlRelativePath;
            FWString WideMtlPath = FullMtlPath.wstring();

            FAssetLoadParams MatParams;
            MatParams.ExplicitType = EAssetType::Material;

            AssetManager->Load(WideMtlPath, MatParams);
        }
    }
    return NewMeshAsset;
}

bool FStaticMeshLoader::ParseObjText(const FSourceRecord& Source, FStaticMeshResource& OutMesh,
                                     TArray<FVertex>& OutVertices) const
{
    if (!Source.bFileBytesLoaded || Source.FileBytes.empty())
        return false;

    FString            FileString(reinterpret_cast<const char*>(Source.FileBytes.data()),
                                  Source.FileBytes.size());
    std::istringstream Stream(FileString);
    FString            Line;

    std::vector<FVector>  TempPositions;
    std::vector<FVector2> TempUVs;
    std::vector<FVector>  TempNormals;

    // 중복 정점 방지를 위한 해시 맵
    TMap<FString, uint32> VertexCache;

    FSubMesh CurrentSubMesh;
    bool     bSubMeshActive = false;

    while (std::getline(Stream, Line))
    {
        if (Line.empty() || Line[0] == '#')
            continue;

        std::istringstream LineStream(Line);
        FString            Header;
        LineStream >> Header;

        if (Header == "mtllib")
        {
            FString MtlFileName;
            while (LineStream >> MtlFileName)
            {
                OutMesh.MaterialLibraryPaths.push_back(MtlFileName);
            }
        }
        else if (Header == "v")
        {
            FVector Pos;
            LineStream >> Pos.X >> Pos.Y >> Pos.Z;
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
            FVector2 UV;
            LineStream >> UV.X >> UV.Y;
            UV.Y = 1.0f - UV.Y; 
            TempUVs.push_back(UV);
        }
        else if (Header == "vn")
        {
            FVector Normal;
            LineStream >> Normal.X >> Normal.Y >> Normal.Z;
            TempNormals.push_back(Normal);
        }
        else if (Header == "usemtl")
        {
            if (bSubMeshActive)
            {
                CurrentSubMesh.IndexCount = static_cast<uint32>(OutMesh.CPU_Indices.size()) -
                                            CurrentSubMesh.StartIndexLocation;
                if (CurrentSubMesh.IndexCount > 0)
                {
                    OutMesh.SubMeshes.push_back(CurrentSubMesh);
                }
            }

            LineStream >> CurrentSubMesh.DefaultMaterialName;
            CurrentSubMesh.StartIndexLocation = static_cast<uint32>(OutMesh.CPU_Indices.size());
            CurrentSubMesh.IndexCount = 0;
            bSubMeshActive = true;
        }
        else if (Header == "f")
        {
            FString VertexToken;
            std::vector<uint32> FaceIndices;

            while (LineStream >> VertexToken)
            {
                auto It = VertexCache.find(VertexToken);
                if (It != VertexCache.end())
                {
                    FaceIndices.push_back(It->second);
                }
                else
                {
                    std::istringstream TokenStream(VertexToken);
                    FString            V_Str, VT_Str, VN_Str;

                    std::getline(TokenStream, V_Str, '/');
                    std::getline(TokenStream, VT_Str, '/');
                    std::getline(TokenStream, VN_Str, '/');

                    FVertex NewVertex = {};
                    if (!V_Str.empty())
                        NewVertex.Position = TempPositions[std::stoi(V_Str) - 1];
                    if (!VT_Str.empty())
                        NewVertex.UV = TempUVs[std::stoi(VT_Str) - 1];
                    if (!VN_Str.empty())
                        NewVertex.Normal = TempNormals[std::stoi(VN_Str) - 1];

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
        {
            OutMesh.SubMeshes.push_back(CurrentSubMesh);
        }
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
    OutMesh.VertexStride = sizeof(FVertex);

    return OutMesh.VertexCount > 0 && OutMesh.IndexCount > 0;
}

bool FStaticMeshLoader::CreateBuffers(const TArray<FVertex>& InVertices,
                                      FStaticMeshResource&   OutMesh) const
{
    if (!RHI || !RHI->GetDevice())
        return false;

    D3D11_BUFFER_DESC VbDesc = {};
    VbDesc.ByteWidth = sizeof(FVertex) * OutMesh.VertexCount;
    VbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    VbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA VbData = {};
    VbData.pSysMem = InVertices.data();

    HRESULT Hr = RHI->GetDevice()->CreateBuffer(&VbDesc, &VbData, &OutMesh.VertexBuffer);
    if (FAILED(Hr))
        return false;

    D3D11_BUFFER_DESC IbDesc = {};
    IbDesc.ByteWidth = sizeof(uint32) * OutMesh.IndexCount;
    IbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    IbDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA IbData = {};
    IbData.pSysMem = OutMesh.CPU_Indices.data();

    Hr = RHI->GetDevice()->CreateBuffer(&IbDesc, &IbData, &OutMesh.IndexBuffer);
    if (FAILED(Hr))
        return false;

    return true;
}

FString FStaticMeshLoader::WidePathToUtf8(const FWString& Path) const
{
    const fs::path      FilePath(Path);
    const std::u8string Utf8Path = FilePath.u8string();
    return FString(reinterpret_cast<const char*>(Utf8Path.data()), Utf8Path.size());
}
