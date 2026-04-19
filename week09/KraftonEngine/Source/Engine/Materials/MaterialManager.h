#pragma once

#include "Core/Singleton.h"
#include "Core/CoreTypes.h"
#include "Render/Types/RenderTypes.h"
#include "SimpleJSON/json.hpp"
#include <memory>

#include "Render/Types/RenderStateTypes.h"

class FMaterialTemplate;
class UMaterial;
struct FMaterialConstantBuffer;

struct FMaterialAssetListItem
{
	FString DisplayName;
	FString FullPath;
};

class FMaterialManager : public TSingleton<FMaterialManager>
{
	friend class TSingleton<FMaterialManager>;

    TMap<FString, FMaterialTemplate*> TemplateCache;    // 셰이더 경로 → Template (공유)
	TMap<FString, UMaterial*> MaterialCache;	//MatFilePath
	TArray<FMaterialAssetListItem> AvailableMaterialFiles;

	ID3D11Device* Device = nullptr;

public:
	~FMaterialManager(); // 선언만 남김

	void Initialize(ID3D11Device* InDevice) { Device = InDevice; }

	// 지정된 디렉토리 내의 모든 머티리얼을 미리 로드
	void LoadAllMaterials(ID3D11Device* Device);

    // UMaterial 생성
	UMaterial* GetOrCreateMaterial(const FString& MatFilePath);
	UMaterial* GetOrCreateStaticMeshMaterial(const FString& MatFilePath);

	void ScanMaterialAssets();
	const TArray<FMaterialAssetListItem>& GetAvailableMaterialFiles() const { return AvailableMaterialFiles; }

	void Release();
private:
	// 셰이더로 Template 생성 또는 캐시에서 반환
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
	
	bool InjectDefaultParameters(json::JSON& JsonData, FMaterialTemplate* Template, UMaterial* Material);
	bool PurgeStaleParameters(json::JSON& JsonData, FMaterialTemplate* Template);
	
	const FString DefaultShaderPath = "Shaders/StaticMeshShader.hlsl";


};