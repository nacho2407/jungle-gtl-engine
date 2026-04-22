#pragma once

#include "Core/Singleton.h"
#include "Core/CoreTypes.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "SimpleJSON/json.hpp"
#include "Materials/MaterialSemantics.h"
#include "Render/Execute/Context/PipelineStateTypes.h"

#include <filesystem>
#include <memory>
#include <vector>

class FMaterialTemplate;
class UMaterial;
struct FMaterialConstantBuffer;

struct FMaterialAssetListItem
{
    FString DisplayName;
    FString FullPath;
};

struct FMaterialFileDependency
{
    FString FullPath;
    std::filesystem::file_time_type LastWriteTime{};
    bool bExists = false;
    uint64 DependencyHash = 0;
};

struct FTemplateCacheEntry
{
    FMaterialTemplate* Template = nullptr;
};

struct FMaterialCacheEntry
{
    UMaterial* Material = nullptr;
    FMaterialFileDependency MaterialFile;
    std::vector<FMaterialFileDependency> TextureFiles;
};

class FMaterialManager : public TSingleton<FMaterialManager>
{
    friend class TSingleton<FMaterialManager>;

    TMap<FString, FTemplateCacheEntry> TemplateCache;
    TMap<FString, FMaterialCacheEntry> MaterialCache;
    TArray<FMaterialAssetListItem> AvailableMaterialFiles;
    TArray<FMaterialAssetListItem> AvailableEditorMaterialFiles;
    TArray<FMaterialTemplate*> RetiredTemplates;
    TArray<UMaterial*> RetiredMaterials;

    ID3D11Device* Device = nullptr;

public:
    ~FMaterialManager();

    void Initialize(ID3D11Device* InDevice) { Device = InDevice; }
    void LoadAllMaterials(ID3D11Device* Device);

    UMaterial* GetOrCreateMaterial(const FString& MatFilePath);
    UMaterial* GetOrCreateStaticMeshMaterial(const FString& MatFilePath);
    UMaterial* GetOrCreateEditorMaterial(const FString& MatFilePath);

    void ScanMaterialAssets();
    const TArray<FMaterialAssetListItem>& GetAvailableMaterialFiles() const { return AvailableMaterialFiles; }
    const TArray<FMaterialAssetListItem>& GetAvailableRuntimeMaterialFiles() const { return AvailableMaterialFiles; }
    const TArray<FMaterialAssetListItem>& GetAvailableEditorMaterialFiles() const { return AvailableEditorMaterialFiles; }

    void Release();

private:
    FMaterialTemplate* GetOrCreateTemplate();

    json::JSON ReadJsonFile(const FString& FilePath) const;
    TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> CreateConstantBuffers(FMaterialTemplate* Template);

    void ApplyParameters(UMaterial* Material, json::JSON& JsonData);
    void ApplyTextures(UMaterial* Material, json::JSON& JsonData, const FString& MatFilePath);

    EBlendState StringToBlendState(const FString& Str) const;
    EDepthStencilState StringToDepthStencilState(const FString& Str) const;
    ERasterizerState StringToRasterizerState(const FString& Str) const;

    void SaveToJSON(json::JSON& JsonData, const FString& MatFilePath);
    bool NormalizeMaterialJson(json::JSON& JsonData, const FString& MaterialPath);

    bool InjectDefaultParameters(json::JSON& JsonData, FMaterialTemplate* Template, UMaterial* Material);
    bool PurgeStaleParameters(json::JSON& JsonData, FMaterialTemplate* Template);

    std::filesystem::path ResolveFullPath(const FString& FilePath) const;
    FString NormalizeCacheKey(const FString& FilePath) const;
    FMaterialFileDependency BuildFileDependency(const std::filesystem::path& FilePath) const;
    bool HasDependencyChanged(const FMaterialFileDependency& Dependency) const;
    bool HasAnyDependencyChanged(const std::vector<FMaterialFileDependency>& Dependencies) const;
    std::filesystem::path ResolveTexturePath(const FString& TexturePath, const FString& MatFilePath) const;
    std::vector<FMaterialFileDependency> CollectTextureDependencies(json::JSON& JsonData, const FString& MatFilePath) const;
    void RetireMaterialCacheEntry(FMaterialCacheEntry& Entry);
};