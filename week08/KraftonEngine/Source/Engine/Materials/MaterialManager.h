#pragma once

#include "Core/Singleton.h"
#include "Core/CoreTypes.h"
#include "Render/Types/RenderTypes.h"
#include "SimpleJSON/json.hpp"
#include "Materials/MaterialSemantics.h"
#include <filesystem>
#include <memory>
#include <vector>

#include "Render/Types/RenderStateTypes.h"

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
    FMaterialFileDependency ShaderFile;
};

struct FMaterialCacheEntry
{
    UMaterial* Material = nullptr;
    FMaterialFileDependency MaterialFile;
    FMaterialFileDependency ShaderFile;
    std::vector<FMaterialFileDependency> TextureFiles;
};

class FMaterialManager : public TSingleton<FMaterialManager>
{
	friend class TSingleton<FMaterialManager>;

    TMap<FString, FTemplateCacheEntry> TemplateCache;     // 정규화된 셰이더 경로 → Template
	TMap<FString, FMaterialCacheEntry> MaterialCache;      // 정규화된 머티리얼 경로 → Material
	TArray<FMaterialAssetListItem> AvailableMaterialFiles;
    TArray<FMaterialTemplate*> RetiredTemplates;
    TArray<UMaterial*> RetiredMaterials;

	ID3D11Device* Device = nullptr;

public:
	~FMaterialManager();

	void Initialize(ID3D11Device* InDevice) { Device = InDevice; }

	void LoadAllMaterials(ID3D11Device* Device);

	UMaterial* GetOrCreateMaterial(const FString& MatFilePath);
	UMaterial* GetOrCreateStaticMeshMaterial(const FString& MatFilePath);

	void ScanMaterialAssets();
	const TArray<FMaterialAssetListItem>& GetAvailableMaterialFiles() const { return AvailableMaterialFiles; }

	void Release();
private:
	FMaterialTemplate* GetOrCreateTemplate(const FString& ShaderPath);

	json::JSON ReadJsonFile(const FString& FilePath) const;

	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> CreateConstantBuffers(FMaterialTemplate* Template);

	void ApplyParameters(UMaterial* Material, json::JSON& JsonData);
	void ApplyTextures(UMaterial* Material, json::JSON& JsonData, const FString& MatFilePath);

	ERenderPass StringToRenderPass(const FString& Str) const;
	EBlendState StringToBlendState(const FString& Str, ERenderPass Pass) const;
	EDepthStencilState StringToDepthStencilState(const FString& Str, ERenderPass Pass) const;
	ERasterizerState StringToRasterizerState(const FString& Str, ERenderPass Pass) const;

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

	const FString DefaultShaderPath = "Shaders/StaticMeshShader.hlsl";
};
