#pragma once

#include "Core/Singleton.h"
#include "Core/CoreTypes.h"
#include "Render/Types/RenderTypes.h"
#include "SimpleJSON/json.hpp"
#include <memory>

class FMaterialTemplate;
class UMaterial;
struct FMaterialConstantBuffer;

class FMaterialManager : public TSingleton<FMaterialManager>
{
	friend class TSingleton<FMaterialManager>;

    TMap<FString, FMaterialTemplate*> TemplateCache;    // 셰이더 경로 → Template (공유)
	TMap<FString, UMaterial*> MaterialCache;	//MatFilePath

	ID3D11Device* Device = nullptr;

public:
	void Initialize(ID3D11Device* InDevice) { Device = InDevice; }

	// 지정된 디렉토리 내의 모든 머티리얼을 미리 로드
	void LoadAllMaterials(ID3D11Device* Device);

    // UMaterial 생성
	UMaterial* GetOrCreateMaterial(const FString& MatFilePath);

private:
	// 셰이더로 Template 생성 또는 캐시에서 반환
	FMaterialTemplate* GetOrCreateTemplate(const FString& ShaderPath, ERenderPass RenderPass);

	json::JSON ReadJsonFile(const FString& FilePath) const;

	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> CreateConstantBuffers(FMaterialTemplate* Template);

	void ApplyParameters(UMaterial* Material, json::JSON& JsonData);
	void ApplyTextures(UMaterial* Material, json::JSON& JsonData);

	ERenderPass StringToRenderPass(const FString& RenderPassStr) const;

	void SaveToJSON(UMaterial* Mat, const FString& MatFilePath);
	
	const FString DefaultShaderPath = "Shaders/Primitive.hlsl";
};