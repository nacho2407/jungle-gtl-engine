#pragma once

#include "Object/Object.h"
#include "Texture.h"
#include "Shader.h"

/**
 * @brief MTL 파일의 머테리얼 데이터를 표현하는 구조체.
 * Obj .mtl 포맷 기준으로 정의했습니다.
 */

struct FMaterial
{
    FString Name;

    FVector AmbientColor   = { 0.2f, 0.2f, 0.2f }; // Ka
    FVector DiffuseColor   = { 0.8f, 0.8f, 0.8f }; // Kd
    FVector SpecularColor  = { 0.0f, 0.0f, 0.0f }; // Ks
    FVector EmissiveColor  = { 0.0f, 0.0f, 0.0f }; // Ke

    float Shininess  = 0.0f; 
    float Opacity    = 1.0f; 
    int   IllumModel = 2;    

	// Texture 정보
    FString DiffuseTexPath;   // map_Kd
	bool	bHasDiffuseTexture = { false };
		 
    FString AmbientTexPath;   // map_Ka
	bool	bHasAmbientTexture = { false };

    FString SpecularTexPath;  // map_Ks
	bool	bHasSpecularTexture = { false };

	FString BumpTexPath;      // map_bump
	bool	bHasBumpTexture = { false };
};

enum class EMaterialParamType
{
	Scalar,
	Vector,
	Texture,
};

struct FMaterialParamValue
{
	FMaterialParamValue() : Type(EMaterialParamType::Scalar), Scalar(0.0f) {}

	EMaterialParamType Type;
	union {
		float Scalar;
		FVector Vector;
		UTexture* Texture;
	};
};

class UMaterialInterface : public UObject
{
public:
	DECLARE_CLASS(UMaterialInterface, UObject)
	virtual void Bind(ID3D11DeviceContext* Context) const = 0;
	virtual bool GetParam(const FString& Name, FMaterialParamValue& OutValue) const = 0;
};

class UMaterial : public UMaterialInterface
{
public:
	DECLARE_CLASS(UMaterial, UMaterialInterface)
	FString Name;
	FMaterial MaterialData;
	TMap<FString, FMaterialParamValue> MaterialParams;
	UShader* ShaderAsset = nullptr;

	void SetParam(const FString& Name, const FMaterialParamValue& Value)
	{
		MaterialParams[Name] = Value;
	}
	virtual bool GetParam(const FString& Name, FMaterialParamValue& OutValue) const
	{
		auto It = MaterialParams.find(Name);
		if (It != MaterialParams.end())
		{
			OutValue = It->second;
			return true;
		}
		return false;
	}

	virtual void Bind(ID3D11DeviceContext* Context) const;

	void GatherAllParams(TMap<FString, FMaterialParamValue>& OutParams) const
	{
		for (const auto& Param : MaterialParams)
		{
			OutParams.insert(MaterialParams.begin(), MaterialParams.end());
		}
	}
};

class UMaterialInstance : public UMaterial
{
public:
	DECLARE_CLASS(UMaterialInstance, UMaterial)
	const UMaterial* Parent = nullptr;
	TMap<FString, FMaterialParamValue> OverridedParams;

	void SetParam(const FString& Name, const FMaterialParamValue& Value)
	{
		OverridedParams[Name] = Value;
	}
	bool GetParam(const FString& Name, FMaterialParamValue& OutValue) const override
	{
		auto It = OverridedParams.find(Name);
		if (It != OverridedParams.end())
		{
			OutValue = It->second;
			return true;
		}
		return Parent ? Parent->GetParam(Name, OutValue) : false;
	}

	void Bind(ID3D11DeviceContext* Context) const override;
};

/**
 * @brief Obj전용 .mtl 파일 파서
 */
class FObjMtlLoader
{
public:
    /**
     * @brief MTL 파일을 파싱하여 머테리얼 맵을 채웁니다.
     * @param FilePath
     * @param OutMaterials 
     * @return 파일 열기 성공 여부 
     */
    static bool Load(const FString& FilePath, TMap<FString, FMaterial>& OutMaterials);
	static bool Load(const FString& FilePath, TMap<FString, UMaterial*>& OutMaterialAssets);
};
