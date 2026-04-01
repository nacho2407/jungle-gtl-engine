#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/RenderAsset/StaticMeshResource.h"
#include "Renderer/RenderAsset/MaterialResource.h"

#include <filesystem>

class FD3D11RHI;
class UAssetManager;
struct FMeshVertexPNCT;

class FEditorAssetImporter
{
public:
    FEditorAssetImporter(FD3D11RHI* InRHI, UAssetManager* InAssetManager);

    bool ImportFile(const FWString& Path);
    bool ImportObj(const FWString& ObjPath);
    bool ImportMtl(const FWString& MtlPath);

private:
    struct FSourceData
    {
        FWString      NormalizedPath;
        TArray<uint8> Bytes;
        uint64        SourceHash = 0;
    };

    bool LoadSource(const FWString& Path, FSourceData& OutSource) const;
    uint64 ComputeFnv1a64(const uint8* Data, size_t Size) const;
    FString WidePathToUtf8(const FWString& Path) const;

    FWString NarrowPathToWidePath(const FString& NarrowPath) const;
    bool     IsAbsoluteWidePath(const FWString& Path) const;
    void     NormalizePathSeparators(FWString& Path) const;
    FWString ResolveSiblingPath(const FWString& BaseFilePath, const FString& RelativePath) const;

    FString  SanitizeFileName(const FString& Name) const;
    std::filesystem::path MakeMeshUAssetPath(const FWString& ObjPath) const;
    std::filesystem::path MakeMaterialUAssetPath(const std::filesystem::path& MtlPath,
                                                 const FString& MaterialName) const;

    bool ParseObjText(const FSourceData& Source, FStaticMesh& OutMesh,
                      TArray<FMeshVertexPNCT>& OutVertices) const;
    bool ParseMtlText(const FSourceData& Source, TMap<FString, FMaterial>& OutMaterials) const;

    bool WriteStaticMeshUAsset(const FSourceData& Source, const FStaticMesh& Mesh,
                               const TArray<FMeshVertexPNCT>& Vertices,
                               const TArray<FString>& MaterialAssetPaths) const;
    bool WriteMaterialUAsset(const FSourceData& Source, const FString& MaterialName,
                             const FMaterial& MaterialData,
                             const std::filesystem::path& OutPath) const;

private:
    FD3D11RHI*     RHI = nullptr;
    UAssetManager* AssetManager = nullptr;
};
