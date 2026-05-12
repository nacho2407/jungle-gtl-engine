#pragma once

#include "Object/Object.h"
#include "Texture.h"
#include "MaterialShaderTypes.h"
#include "ShaderTypes.h"
#include "RenderResources.h"
#include <variant>

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

	FString EmissiveTexPath;  // map_Ke
	bool	bHasEmissiveTexture = { false };

	FString BumpTexPath;      // map_bump
	bool	bHasBumpTexture = { false };
};

enum class EMaterialParamType
{
	Bool,
	Int,
	UInt,
	Float,
	Vector2,
	Vector3,
	Vector4,
	Matrix4,
	Texture,
};

struct FMaterialParamValue
{
	FMaterialParamValue() : Type(EMaterialParamType::Float), Value(0.0f) {}
	FMaterialParamValue(bool InBool) : Type(EMaterialParamType::Bool), Value(InBool) {}
	FMaterialParamValue(int32 InInt) : Type(EMaterialParamType::Int), Value(InInt) {}
	FMaterialParamValue(uint32 InUInt) : Type(EMaterialParamType::UInt), Value(InUInt) {}
	FMaterialParamValue(float InScalar) : Type(EMaterialParamType::Float), Value(InScalar) {}
	FMaterialParamValue(const FVector2& InVector2) : Type(EMaterialParamType::Vector2), Value(InVector2) {}
	FMaterialParamValue(const FVector& InVector3) : Type(EMaterialParamType::Vector3), Value(InVector3) {}
	FMaterialParamValue(const FVector4& InVector4) : Type(EMaterialParamType::Vector4), Value(InVector4) {}
	FMaterialParamValue(const FMatrix& InMatrix4) : Type(EMaterialParamType::Matrix4), Value(InMatrix4) {}
	FMaterialParamValue(UTexture* InTexture) : Type(EMaterialParamType::Texture), Value(InTexture) {}

	EMaterialParamType Type;
	std::variant<bool, int32, uint32, float, FVector2, FVector, FVector4, FMatrix, UTexture*> Value;
};

class UMaterialInterface : public UObject
{
public:
	DECLARE_CLASS(UMaterialInterface, UObject)

	virtual const FString& GetName() const = 0;
	virtual FString& GetNameRef() = 0;
	virtual const FString& GetFilePath() const = 0;
	virtual FString& GetFilePathRef() = 0;

	virtual bool HasDiffuseMap() const = 0;
	virtual bool HasNormalMap() const = 0;
	virtual bool HasSpecularMap() const = 0;
	virtual bool HasEmissiveMap() const = 0;
	virtual bool HasAlphaMask() const = 0;

	// Material은 Shader 파일 경로가 아니라 재질 셰이딩 타입만 소유합니다.
	// 실제 PixelShader 경로와 EntryPoint는 MaterialShaderTypes helper가 변환합니다.
	virtual EMaterialShaderType GetShaderType() const = 0;
	virtual const FString& GetPixelShaderPath() const = 0;
	virtual const FString& GetPixelShaderEntryPoint() const = 0;
	
	// RenderPass는 Program을 먼저 바인딩하고, Material은 상태와 파라미터만 바인딩합니다.
	virtual void BindRenderStates(ID3D11DeviceContext* Context) const = 0;
	virtual void BindParameters(ID3D11DeviceContext* Context, const FPixelShader* PixelShader) const = 0;
	virtual bool GetParam(const FString& Name, FMaterialParamValue& OutValue) const = 0;

	virtual void SetParam(const FString& Name, const FMaterialParamValue& Value) = 0;

	void SetBool(const FString& Name, bool Value) { SetParam(Name, FMaterialParamValue(Value)); }
	void SetInt(const FString& Name, int32 Value) { SetParam(Name, FMaterialParamValue(Value)); }
	void SetUInt(const FString& Name, uint32 Value) { SetParam(Name, FMaterialParamValue(Value)); }
	void SetFloat(const FString& Name, float Value) { SetParam(Name, FMaterialParamValue(Value)); }
	void SetVector2(const FString& Name, const FVector2& Value) { SetParam(Name, FMaterialParamValue(Value)); }
	void SetVector3(const FString& Name, const FVector& Value) { SetParam(Name, FMaterialParamValue(Value)); }
	void SetVector4(const FString& Name, const FVector4& Value) { SetParam(Name, FMaterialParamValue(Value)); }
	void SetMatrix4(const FString& Name, const FMatrix& Value) { SetParam(Name, FMaterialParamValue(Value)); }
	void SetTexture(const FString& Name, UTexture* Value) { SetParam(Name, FMaterialParamValue(Value)); }

	virtual void GatherAllParams(TMap<FString, FMaterialParamValue>& OutParams) const = 0;
};

class UMaterial : public UMaterialInterface
{
public:
	DECLARE_CLASS(UMaterial, UMaterialInterface)

	FString Name;
	FString FilePath;
	FString ImportedName;

	// Material은 HLSL 파일 경로를 직접 저장하지 않고, 재질 셰이딩 타입만 저장합니다.
	// Static/Skeletal 여부와 VS 선택은 RenderCommand의 VertexFactoryType이 결정합니다.
	EMaterialShaderType ShaderType = EMaterialShaderType::SurfaceLit;

	FMaterial MaterialData;
	TMap<FString, FMaterialParamValue> MaterialParams;

	ESamplerType SamplerType = ESamplerType::EST_Linear;
	EDepthStencilType DepthStencilType = EDepthStencilType::Default;
	EBlendType BlendType = EBlendType::Opaque;
	ERasterizerType RasterizerType = ERasterizerType::SolidBackCull;
	D3D11_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	~UMaterial() override;

	const FString& GetName() const override { return Name; }
	FString& GetNameRef() override { return Name; }
	const FString& GetFilePath() const override { return FilePath; }
	FString& GetFilePathRef() override { return FilePath; }

	bool HasDiffuseMap() const override
	{
		if (auto It = MaterialParams.find("bHasDiffuseMap"); It != MaterialParams.end()
			&& It->second.Type == EMaterialParamType::Bool && std::holds_alternative<bool>(It->second.Value))
		{
			return std::get<bool>(It->second.Value);
		}
		if (auto It = MaterialParams.find("DiffuseMap"); It != MaterialParams.end()
			&& It->second.Type == EMaterialParamType::Texture && std::holds_alternative<UTexture*>(It->second.Value))
		{
			return std::get<UTexture*>(It->second.Value) != nullptr;
		}
		return MaterialData.bHasDiffuseTexture;
	}
	bool HasNormalMap() const override
	{
		if (auto It = MaterialParams.find("bHasBumpMap"); It != MaterialParams.end()
			&& It->second.Type == EMaterialParamType::Bool && std::holds_alternative<bool>(It->second.Value))
		{
			return std::get<bool>(It->second.Value);
		}
		if (auto It = MaterialParams.find("BumpMap"); It != MaterialParams.end()
			&& It->second.Type == EMaterialParamType::Texture && std::holds_alternative<UTexture*>(It->second.Value))
		{
			return std::get<UTexture*>(It->second.Value) != nullptr;
		}
		return MaterialData.bHasBumpTexture;
	}
	bool HasSpecularMap() const override
	{
		if (auto It = MaterialParams.find("bHasSpecularMap"); It != MaterialParams.end()
			&& It->second.Type == EMaterialParamType::Bool && std::holds_alternative<bool>(It->second.Value))
		{
			return std::get<bool>(It->second.Value);
		}
		if (auto It = MaterialParams.find("SpecularMap"); It != MaterialParams.end()
			&& It->second.Type == EMaterialParamType::Texture && std::holds_alternative<UTexture*>(It->second.Value))
		{
			return std::get<UTexture*>(It->second.Value) != nullptr;
		}
		return MaterialData.bHasSpecularTexture;
	}
	bool HasEmissiveMap() const override
	{
		if (auto It = MaterialParams.find("bHasEmissiveMap"); It != MaterialParams.end()
			&& It->second.Type == EMaterialParamType::Bool && std::holds_alternative<bool>(It->second.Value))
		{
			return std::get<bool>(It->second.Value);
		}
		if (auto It = MaterialParams.find("EmissiveMap"); It != MaterialParams.end()
			&& It->second.Type == EMaterialParamType::Texture && std::holds_alternative<UTexture*>(It->second.Value))
		{
			return std::get<UTexture*>(It->second.Value) != nullptr;
		}
		return MaterialData.bHasEmissiveTexture;
	}
	bool HasAlphaMask() const override { return false; }
	EMaterialShaderType GetShaderType() const override { return ShaderType; }
	const FString& GetPixelShaderPath() const override { return GetMaterialPixelShaderPath(ShaderType); }
	const FString& GetPixelShaderEntryPoint() const override { return GetMaterialPixelShaderEntryPoint(ShaderType); }

	void SetShaderType(EMaterialShaderType InShaderType)
	{
		ShaderType = InShaderType;
	}

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

	void BindRenderStates(ID3D11DeviceContext* Context) const override;
	void BindParameters(ID3D11DeviceContext* Context, const FPixelShader* PixelShader) const override;

	void ApplyParams(ID3D11DeviceContext* Context, const TMap<FString, FMaterialParamValue>& Params, const FPixelShader* PixelShader) const;

	void GatherAllParams(TMap<FString, FMaterialParamValue>& OutParams) const
	{
		for (const auto& [Key, Param] : MaterialParams)
		{
			OutParams[Key] = Param;
		}
	}

private:
	void ReleaseMaterialConstantBuffer() const;

	mutable ID3D11Buffer* MaterialConstantBuffer = nullptr;
	mutable uint32 MaterialConstantBufferSize = 0;
};

class UMaterialInstance : public UMaterialInterface
{
public:
	DECLARE_CLASS(UMaterialInstance, UMaterialInterface)

	FString Name;
	FString FilePath;

	UMaterial* Parent = nullptr;

	TMap<FString, FMaterialParamValue> OverridedParams;

	const FString& GetName() const override { return Name; }
	FString& GetNameRef() override { return Name; }
	const FString& GetFilePath() const override { return FilePath; }
	FString& GetFilePathRef() override { return FilePath; }

	bool HasDiffuseMap() const override
	{
		if (auto It = OverridedParams.find("bHasDiffuseMap"); It != OverridedParams.end())
		{
			if (It->second.Type == EMaterialParamType::Bool && std::holds_alternative<bool>(It->second.Value))
			{
				return std::get<bool>(It->second.Value);
			}
		}
		if (auto It = OverridedParams.find("DiffuseMap"); It != OverridedParams.end())
		{
			if (It->second.Type == EMaterialParamType::Texture && std::holds_alternative<UTexture*>(It->second.Value))
			{
				return std::get<UTexture*>(It->second.Value) != nullptr;
			}
		}
		return Parent ? Parent->HasDiffuseMap() : false;
	}
	bool HasNormalMap() const override
	{
		if (auto It = OverridedParams.find("bHasBumpMap"); It != OverridedParams.end())
		{
			if (It->second.Type == EMaterialParamType::Bool && std::holds_alternative<bool>(It->second.Value))
			{
				return std::get<bool>(It->second.Value);
			}
		}

		if (auto It = OverridedParams.find("BumpMap"); It != OverridedParams.end())
		{
			if (It->second.Type == EMaterialParamType::Texture && std::holds_alternative<UTexture*>(It->second.Value))
			{
				return std::get<UTexture*>(It->second.Value) != nullptr;
			}
		}

		return Parent ? Parent->HasNormalMap() : false;
	}
	bool HasSpecularMap() const override
	{
		if (auto It = OverridedParams.find("bHasSpecularMap"); It != OverridedParams.end())
		{
			if (It->second.Type == EMaterialParamType::Bool && std::holds_alternative<bool>(It->second.Value))
			{
				return std::get<bool>(It->second.Value);
			}
		}
		if (auto It = OverridedParams.find("SpecularMap"); It != OverridedParams.end())
		{
			if (It->second.Type == EMaterialParamType::Texture && std::holds_alternative<UTexture*>(It->second.Value))
			{
				return std::get<UTexture*>(It->second.Value) != nullptr;
			}
		}
		return Parent ? Parent->HasSpecularMap() : false;
	}
	bool HasEmissiveMap() const override
	{
		if (auto It = OverridedParams.find("bHasEmissiveMap"); It != OverridedParams.end())
		{
			if (It->second.Type == EMaterialParamType::Bool && std::holds_alternative<bool>(It->second.Value))
			{
				return std::get<bool>(It->second.Value);
			}
		}
		if (auto It = OverridedParams.find("EmissiveMap"); It != OverridedParams.end())
		{
			if (It->second.Type == EMaterialParamType::Texture && std::holds_alternative<UTexture*>(It->second.Value))
			{
				return std::get<UTexture*>(It->second.Value) != nullptr;
			}
		}
		return Parent ? Parent->HasEmissiveMap() : false;
	}
	bool HasAlphaMask() const override { return Parent ? Parent->HasAlphaMask() : false; }
	EMaterialShaderType GetShaderType() const override
	{
		return Parent ? Parent->GetShaderType() : EMaterialShaderType::SurfaceLit;
	}
	const FString& GetPixelShaderPath() const override
	{
		return GetMaterialPixelShaderPath(GetShaderType());
	}
	const FString& GetPixelShaderEntryPoint() const override
	{
		return GetMaterialPixelShaderEntryPoint(GetShaderType());
	}

	static UMaterialInstance* Create(UMaterial* Material)
	{
		UMaterialInstance* Instance = new UMaterialInstance();
		Instance->Parent = Material;
		return Instance;
	}

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

	void BindRenderStates(ID3D11DeviceContext* Context) const override;
	void BindParameters(ID3D11DeviceContext* Context, const FPixelShader* PixelShader) const override;

	void GatherAllParams(TMap<FString, FMaterialParamValue>& OutParams) const
	{
		if (Parent)
		{
			Parent->GatherAllParams(OutParams);
		}

		for (const auto& [Key, Param] : OverridedParams)
		{
			OutParams[Key] = Param;
		}
	}
};
