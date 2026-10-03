#include "Core/MaterialSerializationService.h"

#include "Core/AssetPathPolicy.h"
#include "Core/Logging/Log.h"
#include "Core/MaterialResourceCache.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Render/Resource/Material.h"
#include "Render/Resource/Texture.h"
#include "SimpleJSON/json.hpp"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace
{
	using json::JSON;

	JSON SerializeMaterialParam(const FString& ParamName, const FMaterialParamValue& ParamValue)
	{
		JSON Param = JSON::Make(JSON::Class::Object);
		Param["Name"] = ParamName;
		if (std::holds_alternative<bool>(ParamValue.Value))
		{
			Param["Type"] = "Bool";
			Param["Value"] = std::get<bool>(ParamValue.Value);
		}
		else if (std::holds_alternative<int>(ParamValue.Value))
		{
			Param["Type"] = "Int";
			Param["Value"] = std::get<int>(ParamValue.Value);
		}
		else if (std::holds_alternative<uint32>(ParamValue.Value))
		{
			Param["Type"] = "UInt";
			Param["Value"] = std::get<uint32>(ParamValue.Value);
		}
		else if (std::holds_alternative<float>(ParamValue.Value))
		{
			Param["Type"] = "Float";
			Param["Value"] = std::get<float>(ParamValue.Value);
		}
		else if (std::holds_alternative<FVector2>(ParamValue.Value))
		{
			const FVector2& Vec = std::get<FVector2>(ParamValue.Value);
			Param["Type"] = "Vector2";
			Param["Value"] = JSON::Make(JSON::Class::Array);
			Param["Value"].append(Vec.X);
			Param["Value"].append(Vec.Y);
		}
		else if (std::holds_alternative<FVector>(ParamValue.Value))
		{
			const FVector& Vec = std::get<FVector>(ParamValue.Value);
			Param["Type"] = "Vector3";
			Param["Value"] = JSON::Make(JSON::Class::Array);
			Param["Value"].append(Vec.X);
			Param["Value"].append(Vec.Y);
			Param["Value"].append(Vec.Z);
		}
		else if (std::holds_alternative<FVector4>(ParamValue.Value))
		{
			const FVector4& Vec = std::get<FVector4>(ParamValue.Value);
			Param["Type"] = "Vector4";
			Param["Value"] = JSON::Make(JSON::Class::Array);
			Param["Value"].append(Vec.X);
			Param["Value"].append(Vec.Y);
			Param["Value"].append(Vec.Z);
			Param["Value"].append(Vec.W);
		}
		else if (std::holds_alternative<FMatrix>(ParamValue.Value))
		{
			const FMatrix& Mat = std::get<FMatrix>(ParamValue.Value);
			Param["Type"] = "Matrix4";
			JSON MatArray = JSON::Make(JSON::Class::Array);
			for (int Row = 0; Row < 4; ++Row)
			{
				JSON RowArray = JSON::Make(JSON::Class::Array);
				for (int Col = 0; Col < 4; ++Col)
				{
					RowArray.append(Mat.M[Row][Col]);
				}
				MatArray.append(RowArray);
			}
			Param["Value"] = MatArray;
		}
		else if (std::holds_alternative<UTexture*>(ParamValue.Value))
		{
			UTexture* Texture = std::get<UTexture*>(ParamValue.Value);
			Param["Type"] = "Texture";
			Param["Value"] = Texture ? Texture->GetFilePath() : "";
		}
		return Param;
	}

	void ApplyTypedParam(UMaterialInstance* MaterialInstance, const FString& ParamName, const FString& Type, JSON& Param, FResourceManager& ResourceManager)
	{
		if (Type == "Bool")
		{
			MaterialInstance->SetParam(ParamName, FMaterialParamValue(Param["Value"].ToBool()));
		}
		else if (Type == "Int")
		{
			MaterialInstance->SetParam(ParamName, FMaterialParamValue(static_cast<int32>(Param["Value"].ToInt())));
		}
		else if (Type == "UInt")
		{
			MaterialInstance->SetParam(ParamName, FMaterialParamValue(static_cast<uint32>(Param["Value"].ToInt())));
		}
		else if (Type == "Float")
		{
			MaterialInstance->SetParam(ParamName, FMaterialParamValue(static_cast<float>(Param["Value"].ToFloat())));
		}
		else if (Type == "Vector2" || Type == "FVector2")
		{
			MaterialInstance->SetParam(ParamName, FMaterialParamValue(FVector2(
				static_cast<float>(Param["Value"][0].ToFloat()),
				static_cast<float>(Param["Value"][1].ToFloat()))));
		}
		else if (Type == "Vector3" || Type == "FVector3")
		{
			MaterialInstance->SetParam(ParamName, FMaterialParamValue(FVector(
				static_cast<float>(Param["Value"][0].ToFloat()),
				static_cast<float>(Param["Value"][1].ToFloat()),
				static_cast<float>(Param["Value"][2].ToFloat()))));
		}
		else if (Type == "Vector4" || Type == "FVector4")
		{
			MaterialInstance->SetParam(ParamName, FMaterialParamValue(FVector4(
				static_cast<float>(Param["Value"][0].ToFloat()),
				static_cast<float>(Param["Value"][1].ToFloat()),
				static_cast<float>(Param["Value"][2].ToFloat()),
				static_cast<float>(Param["Value"][3].ToFloat()))));
		}
		else if (Type == "Matrix4")
		{
			FMatrix Value;
			for (int Row = 0; Row < 4; ++Row)
			{
				for (int Col = 0; Col < 4; ++Col)
				{
					Value.M[Row][Col] = static_cast<float>(Param["Value"][Row][Col].ToFloat());
				}
			}
			MaterialInstance->SetParam(ParamName, FMaterialParamValue(Value));
		}
		else if (Type == "Texture")
		{
			const FString TexPath = Param["Value"].ToString();
			UTexture* Texture = ResourceManager.LoadTexture(TexPath);
			if (Texture)
			{
				MaterialInstance->SetParam(ParamName, FMaterialParamValue(Texture));
			}
		}
	}

	void ApplyMaterialDataVector(UMaterial* Material, const FString& ParamName, const FVector& Value)
	{
		if (ParamName == "AmbientColor")
		{
			Material->MaterialData.AmbientColor = Value;
		}
		else if (ParamName == "DiffuseColor")
		{
			Material->MaterialData.DiffuseColor = Value;
		}
		else if (ParamName == "SpecularColor")
		{
			Material->MaterialData.SpecularColor = Value;
		}
		else if (ParamName == "EmissiveColor")
		{
			Material->MaterialData.EmissiveColor = Value;
		}
	}

	void ApplyMaterialDataTexture(UMaterial* Material, const FString& ParamName, const FString& TexPath)
	{
		const FString NormalizedTexPath = FPaths::Normalize(TexPath);
		if (ParamName == "DiffuseMap")
		{
			Material->MaterialData.DiffuseTexPath = NormalizedTexPath;
			Material->MaterialData.bHasDiffuseTexture = true;
			Material->SetParam("bHasDiffuseMap", FMaterialParamValue(true));
		}
		else if (ParamName == "SpecularMap")
		{
			Material->MaterialData.SpecularTexPath = NormalizedTexPath;
			Material->MaterialData.bHasSpecularTexture = true;
			Material->SetParam("bHasSpecularMap", FMaterialParamValue(true));
		}
		else if (ParamName == "EmissiveMap")
		{
			Material->MaterialData.EmissiveTexPath = NormalizedTexPath;
			Material->MaterialData.bHasEmissiveTexture = true;
			Material->SetParam("bHasEmissiveMap", FMaterialParamValue(true));
		}
		else if (ParamName == "AmbientMap")
		{
			Material->MaterialData.AmbientTexPath = NormalizedTexPath;
			Material->MaterialData.bHasAmbientTexture = true;
			Material->SetParam("bHasAmbientMap", FMaterialParamValue(true));
		}
		else if (ParamName == "BumpMap")
		{
			Material->MaterialData.BumpTexPath = NormalizedTexPath;
			Material->MaterialData.bHasBumpTexture = true;
			Material->SetParam("bHasBumpMap", FMaterialParamValue(true));
		}
	}

	void ApplyTypedParam(UMaterial* Material, const FString& ParamName, const FString& Type, JSON& Param, FResourceManager& ResourceManager)
	{
		if (Type == "Bool")
		{
			Material->SetParam(ParamName, FMaterialParamValue(Param["Value"].ToBool()));
		}
		else if (Type == "Int")
		{
			Material->SetParam(ParamName, FMaterialParamValue(static_cast<int32>(Param["Value"].ToInt())));
		}
		else if (Type == "UInt")
		{
			Material->SetParam(ParamName, FMaterialParamValue(static_cast<uint32>(Param["Value"].ToInt())));
		}
		else if (Type == "Float")
		{
			Material->SetParam(ParamName, FMaterialParamValue(static_cast<float>(Param["Value"].ToFloat())));
		}
		else if (Type == "Vector2" || Type == "FVector2")
		{
			Material->SetParam(ParamName, FMaterialParamValue(FVector2(
				static_cast<float>(Param["Value"][0].ToFloat()),
				static_cast<float>(Param["Value"][1].ToFloat()))));
		}
		else if (Type == "Vector3" || Type == "FVector3")
		{
			FVector Value(
				static_cast<float>(Param["Value"][0].ToFloat()),
				static_cast<float>(Param["Value"][1].ToFloat()),
				static_cast<float>(Param["Value"][2].ToFloat()));
			Material->SetParam(ParamName, FMaterialParamValue(Value));
			ApplyMaterialDataVector(Material, ParamName, Value);
		}
		else if (Type == "Vector4" || Type == "FVector4")
		{
			Material->SetParam(ParamName, FMaterialParamValue(FVector4(
				static_cast<float>(Param["Value"][0].ToFloat()),
				static_cast<float>(Param["Value"][1].ToFloat()),
				static_cast<float>(Param["Value"][2].ToFloat()),
				static_cast<float>(Param["Value"][3].ToFloat()))));
		}
		else if (Type == "Matrix4")
		{
			FMatrix Value;
			for (int Row = 0; Row < 4; ++Row)
			{
				for (int Col = 0; Col < 4; ++Col)
				{
					Value.M[Row][Col] = static_cast<float>(Param["Value"][Row][Col].ToFloat());
				}
			}
			Material->SetParam(ParamName, FMaterialParamValue(Value));
		}
		else if (Type == "Texture")
		{
			const FString TexPath = Param["Value"].ToString();
			UTexture* Texture = ResourceManager.LoadTexture(TexPath);
			if (Texture)
			{
				Material->SetParam(ParamName, FMaterialParamValue(Texture));
				ApplyMaterialDataTexture(Material, ParamName, TexPath);
			}
		}
	}
}

FMaterialSerializationService::FMaterialSerializationService(FResourceManager& InResourceManager)
	: ResourceManager(InResourceManager)
{
}

bool FMaterialSerializationService::SerializeMaterial(const FString& MatFilePath, const UMaterial* Material)
{
	using json::JSON;
	const FString NormalizedMatFilePath = FPaths::Normalize(MatFilePath);
	JSON Root = JSON::Make(JSON::Class::Object);
	Root["Name"] = Material->Name;
	if (!Material->ImportedName.empty())
	{
		Root["ImportedName"] = Material->ImportedName;
	}
	Root["ShaderType"] = ToString(Material->GetShaderType());

	JSON Params = JSON::Make(JSON::Class::Array);
	for (const auto& [ParamName, ParamValue] : Material->MaterialParams)
	{
		Params.append(SerializeMaterialParam(ParamName, ParamValue));
	}
	Root["Params"] = Params;

	std::ofstream OutFile(FPaths::ToWide(NormalizedMatFilePath));
	if (!OutFile.is_open())
	{
		UE_LOG_ERROR("Failed to open material file for writing: %s", NormalizedMatFilePath.c_str());
		return false;
	}
	OutFile << Root.dump(4);
	return true;
}

bool FMaterialSerializationService::SerializeMaterialInstance(const FString& MatInstFilePath, const UMaterialInstance* MaterialInstance)
{
	using json::JSON;
	const FString NormalizedMatInstFilePath = FPaths::Normalize(MatInstFilePath);
	JSON Root = JSON::Make(JSON::Class::Object);

	// ?대쫫?먮뒗 ?댁젣 ?뚯씪 寃쎈줈瑜??ｋ뒗 寃껋쑝濡??듭씪. ?뚯씪 寃쎈줈媛 ?놁쑝硫?湲곗〈 諛⑹떇?濡??대쫫???ｌ쓬
	Root["Name"] = MaterialInstance->GetFilePath().empty() ? NormalizedMatInstFilePath : FPaths::Normalize(MaterialInstance->GetFilePath());
	Root["Parent"] = (MaterialInstance->Parent && !MaterialInstance->Parent->GetFilePath().empty())
		? FPaths::Normalize(MaterialInstance->Parent->GetFilePath())
		: (MaterialInstance->Parent ? MaterialInstance->Parent->Name : "");

	JSON Params = JSON::Make(JSON::Class::Array);
	for (const auto& [ParamName, ParamValue] : MaterialInstance->OverridedParams)
	{
		Params.append(SerializeMaterialParam(ParamName, ParamValue));
	}
	Root["OverridedParams"] = Params;

	std::error_code Ec;
	fs::create_directories(fs::path(FPaths::ToWide(NormalizedMatInstFilePath)).parent_path(), Ec);
	std::ofstream OutFile(FPaths::ToWide(NormalizedMatInstFilePath));
	if (!OutFile.is_open())
	{
		UE_LOG_ERROR("Failed to open material instance file for writing: %s", NormalizedMatInstFilePath.c_str());
		return false;
	}
	OutFile << Root.dump(4);
	return true;
}

bool FMaterialSerializationService::DeserializeMaterial(const FString& MatFilePath)
{
	using json::JSON;
	const FString NormalizedMatFilePath = FPaths::Normalize(MatFilePath);

	std::ifstream MatFile(FPaths::ToWide(NormalizedMatFilePath));
	if (!MatFile.is_open())
	{
		UE_LOG_ERROR("Failed to open material file: %s", NormalizedMatFilePath.c_str());
		return false;
	}

	FString FileContent((std::istreambuf_iterator<char>(MatFile)), std::istreambuf_iterator<char>());
	JSON Root = JSON::Load(FileContent);

	if (Root.hasKey("Parent"))
	{
		const FString InstancePath = NormalizedMatFilePath;
		const FString ParentIdentifier = Root["Parent"].ToString();
		UMaterial* ParentMat = ResourceManager.GetMaterial(ParentIdentifier);

		if (!ParentMat)
		{
			ParentMat = ResourceManager.GetMaterial(FPaths::Normalize(ParentIdentifier));
		}

		const FString NormalizedParentIdentifier = FPaths::Normalize(ParentIdentifier);
		if (!ParentMat && FAssetPathPolicy::IsSerializedMaterialAssetPath(NormalizedParentIdentifier) && FAssetPathPolicy::FileExists(NormalizedParentIdentifier))
		{
			DeserializeMaterial(NormalizedParentIdentifier);
			ParentMat = ResourceManager.GetMaterial(NormalizedParentIdentifier);
			if (!ParentMat)
			{
				ParentMat = ResourceManager.GetMaterial(ParentIdentifier);
			}
		}

		if (!ParentMat)
		{
			UE_LOG_WARNING("Parent material not found: %s", ParentIdentifier.c_str());
			return false;
		}

		UMaterialInstance* MatInstance = ResourceManager.CreateMaterialInstance(InstancePath, ParentMat);

		for (auto& Param : Root["OverridedParams"].ArrayRange())
		{
			const FString ParamName = Param["Name"].ToString();
			const FString Type = Param["Type"].ToString();
			ApplyTypedParam(MatInstance, ParamName, Type, Param, ResourceManager);
		}

		ResourceManager.MaterialCache.RegisterMaterialInstance(InstancePath, MatInstance);
		return true;
	}

	const FString MatName = Root["Name"].ToString();
	EMaterialShaderType ShaderType = EMaterialShaderType::SurfaceLit;
	if (!TryParseMaterialShaderType(Root["ShaderType"].ToString(), ShaderType))
	{
		UE_LOG_ERROR("Invalid or missing material ShaderType: %s", NormalizedMatFilePath.c_str());
		return false;
	}

	UMaterial* Material = ResourceManager.GetOrCreateMaterial(MatName, NormalizedMatFilePath, ShaderType);
	Material->SetShaderType(ShaderType);
	if (Root.hasKey("ImportedName"))
	{
		Material->ImportedName = Root["ImportedName"].ToString();
		if (!Material->ImportedName.empty() && !ResourceManager.MaterialCache.ContainsMaterialKey(Material->ImportedName))
		{
			ResourceManager.MaterialCache.RegisterMaterial(Material->ImportedName, Material);
		}
	}
	ResourceManager.MaterialCache.RegisterMaterial(NormalizedMatFilePath, Material);
	ResourceManager.MaterialCache.RegisterMaterial(MatName, Material);
	Material->SetParam("AmbientColor", FMaterialParamValue(Material->MaterialData.AmbientColor));
	Material->SetParam("DiffuseColor", FMaterialParamValue(Material->MaterialData.DiffuseColor));
	Material->SetParam("SpecularColor", FMaterialParamValue(Material->MaterialData.SpecularColor));
	Material->SetParam("EmissiveColor", FMaterialParamValue(Material->MaterialData.EmissiveColor));
	Material->SetParam("Shininess", FMaterialParamValue(Material->MaterialData.Shininess));
	Material->SetParam("Opacity", FMaterialParamValue(Material->MaterialData.Opacity));
	Material->SetParam("ScrollUV", FMaterialParamValue(FVector2(0.0f, 0.0f)));

	for (auto& Param : Root["Params"].ArrayRange())
	{
		const FString ParamName = Param["Name"].ToString();
		const FString Type = Param["Type"].ToString();
		ApplyTypedParam(Material, ParamName, Type, Param, ResourceManager);
	}

	ResourceManager.MaterialCache.RegisterMaterial(MatName, Material);
	return true;
}
