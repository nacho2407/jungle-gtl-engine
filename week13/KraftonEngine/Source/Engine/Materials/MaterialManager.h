#pragma once

#include "Core/Singleton.h"
#include "Core/Types/CoreTypes.h"
#include "Render/Types/RenderTypes.h"
#include "SimpleJSON/json.hpp"
#include <memory>

#include "Materials/Graph/MaterialGraphTypes.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Types/RenderStateTypes.h"

namespace MatKeys
{
	static constexpr const char* PathFileName = "PathFileName";
	static constexpr const char* ShaderPath = "ShaderPath";
	static constexpr const char* Version = "Version";
	static constexpr const char* MaterialGuid = "MaterialGuid";
	static constexpr const char* Domain = "Domain";
	static constexpr const char* RenderPass = "RenderPass";
	static constexpr const char* BlendState = "BlendState";
	static constexpr const char* DepthStencilState = "DepthStencilState";
	static constexpr const char* RasterizerState = "RasterizerState";
	static constexpr const char* GeneratedShaderPath = "GeneratedShaderPath";
	static constexpr const char* Graph = "Graph";
	static constexpr const char* Compiled = "Compiled";
	static constexpr const char* GraphHash = "GraphHash";
	static constexpr const char* GeneratorVersion = "GeneratorVersion";
	static constexpr const char* Parameters = "Parameters";
	static constexpr const char* Textures = "Textures";
	static constexpr const char* ReceiveLighting = "ReceiveLighting";
}

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

	// 텍스처 슬롯이 비어있을 때 sample하면 (0,0,0,0)이 되어 alpha-clip에 걸리므로
	// 1x1 흰색 SRV를 fallback으로 보관해두고 슬롯이 null일 때 바인딩.
	struct ID3D11Texture2D* FallbackWhiteTexture = nullptr;
	struct ID3D11ShaderResourceView* FallbackWhiteSRV = nullptr;

public:
	~FMaterialManager(); // 선언만 남김

	void Initialize(ID3D11Device* InDevice);

	// null SRV 슬롯에 바인딩할 흰색 1x1 fallback. 시스템 라이프타임 동안 유효.
	ID3D11ShaderResourceView* GetFallbackWhiteSRV() const { return FallbackWhiteSRV; }

	// 지정된 디렉토리 내의 모든 머티리얼을 미리 로드
	void LoadAllMaterials(ID3D11Device* Device);

	// UMaterial 생성
	UMaterial* GetOrCreateMaterial(const FString& MatFilePath);
	UMaterial* ReloadMaterial(const FString& MatFilePath);
	void InvalidateMaterial(const FString& MatFilePath);
	bool SaveMaterialAsset(UMaterial* Material);
	bool SaveMaterialJson(const FString& MatFilePath, const json::JSON& JsonData);
	bool CompileMaterialGraph(const FString& MatFilePath, json::JSON& InOutJson, FString* OutError);

	void ScanMaterialAssets();
	const TArray<FMaterialAssetListItem>& GetAvailableMaterialFiles() const { return AvailableMaterialFiles; }

	void Release();
private:
	// 셰이더로 Template 생성 또는 캐시에서 반환
	FMaterialTemplate* GetOrCreateTemplate(const FString& ShaderPath);
	FMaterialTemplate* GetOrCreateTemplate(const struct FShaderKey& Key);

	json::JSON ReadJsonFile(const FString& FilePath) const;
	bool LoadMaterialFromJson(const FString& MatFilePath, json::JSON& JsonData, UMaterial* ReuseMaterial, UMaterial*& OutMaterial);

	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> CreateConstantBuffers(FMaterialTemplate* Template);

	void ApplyParameters(UMaterial* Material, json::JSON& JsonData);
	void ApplyTextures(UMaterial* Material, json::JSON& JsonData);

	EMaterialDomain StringToDomain(const FString& Str) const;
	EShaderVertexFactory DomainToVertexFactory(EMaterialDomain Domain) const;
	ERenderPass StringToRenderPass(const FString& Str) const;
	EBlendState StringToBlendState(const FString& Str, ERenderPass Pass) const;
	EDepthStencilState StringToDepthStencilState(const FString& Str, ERenderPass Pass) const;
	ERasterizerState StringToRasterizerState(const FString& Str, ERenderPass Pass) const;

	void SaveToJSON(json::JSON& JsonData, const FString& MatFilePath);
	
	bool InjectDefaultParameters(json::JSON& JsonData, FMaterialTemplate* Template, UMaterial* Material);
	bool PurgeStaleParameters(json::JSON& JsonData, FMaterialTemplate* Template);
	void EnsureGraphMaterialJsonDefaults(const FString& MatFilePath, json::JSON& JsonData);
	
	const FString DefaultShaderPath = "Shaders/Geometry/UberLit.hlsl";


};
