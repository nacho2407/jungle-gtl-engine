// 머티리얼 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/Singleton.h"
#include "Core/CoreTypes.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "SimpleJSON/json.hpp"
#include "Materials/MaterialSemantics.h"

#include <filesystem>
#include <memory>
#include <vector>

class FMaterialTemplate;
class UMaterial;
struct FMaterialConstantBuffer;

// FMaterialAssetListItem는 머티리얼 파라미터와 렌더 리소스를 다룹니다.
struct FMaterialAssetListItem
{
    FString DisplayName;
    FString FullPath;
};

// FMaterialFileDependency는 머티리얼 파라미터와 렌더 리소스를 다룹니다.
struct FMaterialFileDependency
{
    FString FullPath;
    std::filesystem::file_time_type LastWriteTime{};
    bool bExists = false;
    uint64 DependencyHash = 0;
};

// FTemplateCacheEntry는 머티리얼 처리에 필요한 데이터를 묶는 구조체입니다.
struct FTemplateCacheEntry
{
    FMaterialTemplate* Template = nullptr;
};

// FMaterialCacheEntry는 머티리얼 파라미터와 렌더 리소스를 다룹니다.
struct FMaterialCacheEntry
{
    UMaterial* Material = nullptr;
    FMaterialFileDependency MaterialFile;
    std::vector<FMaterialFileDependency> TextureFiles;
};

// FMaterialManager는 관련 객체의 생성, 조회, 수명 관리를 담당합니다.
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

    std::filesystem::path ResolveFullPath(const FString& FilePath) const;
    FString NormalizeCacheKey(const FString& FilePath) const;
    FMaterialFileDependency BuildFileDependency(const std::filesystem::path& FilePath) const;
    bool HasDependencyChanged(const FMaterialFileDependency& Dependency) const;
    bool HasAnyDependencyChanged(const std::vector<FMaterialFileDependency>& Dependencies) const;
    std::filesystem::path ResolveTexturePath(const FString& TexturePath, const FString& MatFilePath) const;
    std::vector<FMaterialFileDependency> CollectTextureDependencies(json::JSON& JsonData, const FString& MatFilePath) const;
    void RetireMaterialCacheEntry(FMaterialCacheEntry& Entry);
};
