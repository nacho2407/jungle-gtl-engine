#pragma once

#include "Core/Singleton.h"
#include "Core/Types/CoreTypes.h"
#include "Render/Types/RenderTypes.h"
#include "SimpleJSON/json.hpp"
#include <memory>

#include "Render/Types/RenderStateTypes.h"

namespace MatKeys
{
	static constexpr const char* PathFileName = "PathFileName";
	static constexpr const char* ShaderPath = "ShaderPath";
	static constexpr const char* RenderPass = "RenderPass";
	static constexpr const char* BlendState = "BlendState";
	static constexpr const char* DepthStencilState = "DepthStencilState";
	static constexpr const char* RasterizerState = "RasterizerState";
	static constexpr const char* ParentMaterial = "ParentMaterial";
	static constexpr const char* Parameters = "Parameters";
	static constexpr const char* Textures = "Textures";
	static constexpr const char* ShadowMode = "ShadowMode";	
}

class FMaterialTemplate;
class FShader;
class UMaterial;
class UMaterialInterface;
class UMaterialInstance;
struct FMaterialConstantBuffer;
enum class EMaterialShadowMode : uint8;

class FReferenceCollector;

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
	TMap<FString, UMaterialInstance*> MaterialInstanceCache;
	TMap<uint32, UMaterial*> TransientMaterialRegistry;
	TArray<FMaterialAssetListItem> AvailableMaterialFiles;
	uint32 NextTransientMaterialId = 1;

	ID3D11Device* Device = nullptr;

public:
	~FMaterialManager(); // 선언만 남김

	void Initialize(ID3D11Device* InDevice) { Device = InDevice; }

	// 지정된 디렉토리 내의 모든 머티리얼을 미리 로드
	void LoadAllMaterials(ID3D11Device* Device);

	// UMaterial 생성
	UMaterial* GetOrCreateMaterial(const FString& MatFilePath);
	UMaterialInstance* GetOrCreateMaterialInstance(const FString& MatInstFilePath);

	UMaterialInterface* GetOrCreateMaterialInterface(const FString& AssetPath);
	UMaterial* CreateTransientMaterial(ERenderPass InPass, EBlendState InBlend,
		EDepthStencilState InDepth = EDepthStencilState::Default,
		ERasterizerState InRaster = ERasterizerState::SolidBackCull,
		FShader* InShader = nullptr);
	void DestroyTransientMaterial(UMaterial* Material);

	void ScanMaterialAssets();
	const TArray<FMaterialAssetListItem>& GetAvailableMaterialFiles() const { return AvailableMaterialFiles; }

	void Release();

	void AddReferencedObjects(FReferenceCollector& Collector);

private:
	// 셰이더로 Template 생성 또는 캐시에서 반환
	FMaterialTemplate* GetOrCreateTemplate(const FString& ShaderPath);

	json::JSON ReadJsonFile(const FString& FilePath) const;

	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> CreateConstantBuffers(FMaterialTemplate* Template);

	void ApplyParameters(UMaterialInterface* Material, json::JSON& JsonData);
	void ApplyTextures(UMaterialInterface* Material, json::JSON& JsonData);

	ERenderPass StringToRenderPass(const FString& Str) const;
	EBlendState StringToBlendState(const FString& Str, ERenderPass Pass) const;
	EDepthStencilState StringToDepthStencilState(const FString& Str, ERenderPass Pass) const;
	ERasterizerState StringToRasterizerState(const FString& Str, ERenderPass Pass) const;

	EMaterialShadowMode StringToShadowMode(const FString& Str) const;
	const char* ShadowModeToString(EMaterialShadowMode Mode) const;

	void SaveToJSON(json::JSON& JsonData, const FString& MatFilePath);
	
	bool InjectDefaultParameters(json::JSON& JsonData, FMaterialTemplate* Template, UMaterial* Material);
	bool PurgeStaleParameters(json::JSON& JsonData, FMaterialTemplate* Template);
	
	const FString DefaultShaderPath = "Shaders/Geometry/UberLit.hlsl";


};
