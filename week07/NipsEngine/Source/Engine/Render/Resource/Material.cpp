#include "Material.h"
#include "Asset/FileUtils.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"

#include <filesystem>

DEFINE_CLASS(UMaterialInterface, UObject)
DEFINE_CLASS(UMaterial, UMaterialInterface)
DEFINE_CLASS(UMaterialInstance, UMaterial)

bool FObjMtlLoader::Load(const FString& FilePath, TMap<FString, FMaterial>& OutMaterials)
{
    std::ifstream File(std::filesystem::path(FPaths::ToWide(FilePath)));
	if (!File.is_open())
	{
		return false;
	}

	// 한글 경로 안전을 위해 wide string 기반으로 filesystem 연산 수행
	std::filesystem::path MtlDir = std::filesystem::path(FPaths::ToWide(FilePath)).parent_path();

	auto ResolveTexPath = [&](std::istringstream& InISS) -> FString
		{
			FString RelPath;

			InISS >> RelPath;
			if (RelPath.empty())
			{
				return {};
			}

			std::filesystem::path FileName = std::filesystem::path(FPaths::ToWide(RelPath)).filename();

			FString outTexPath = "";
			FFileUtils::FindFileRecursively(
				FPaths::ToUtf8(MtlDir.generic_wstring()),
				FPaths::ToUtf8(FileName.generic_wstring()),
				outTexPath);

			// 기존: std::filesystem::path TexPath = (MtlDir / outTexPath).lexically_normal();
			// 변경: UTF-8 문자열(outTexPath)을 wide로 명시 변환 후 결합
			std::filesystem::path TexPath = (MtlDir / std::filesystem::path(FPaths::ToWide(outTexPath))).lexically_normal();

			return FPaths::ToUtf8(TexPath.generic_wstring());
		};

    FMaterial* Current = nullptr;
    FString    Line;

	auto ParseFVector = [](std::istringstream& InISS) -> FVector
		{
			FVector Vector;
			InISS >> Vector.X >> Vector.Y >> Vector.Z;
			return Vector;
		};

    while (std::getline(File, Line))
    {
        Line = StringUtils::Trim(Line);
        if (Line.empty() || Line.front() == '#')
            continue;

        std::istringstream ISS(Line);
        FString Token;
		ISS >> Token;

		if (Token == "newmtl")
		{
			FString MatName;
			ISS >> MatName;
			OutMaterials[MatName] = FMaterial{};
			Current = &OutMaterials[MatName];
			Current->Name = MatName;
		}
		// newmtl 이전 라인은 무시
		else if (!Current)
		{
			continue;
		}
		// 색상
		else if (Token == "Ka")
		{
			Current->AmbientColor = ParseFVector(ISS);
		}
		else if (Token == "Kd")
		{
			Current->DiffuseColor = ParseFVector(ISS);
		}
		else if (Token == "Ks")
		{
			Current->SpecularColor = ParseFVector(ISS);
		}
		else if (Token == "Ke")
		{
			Current->EmissiveColor = ParseFVector(ISS);
		}
		// 광택 집중도
		else if (Token == "Ns")
		{
			ISS >> Current->Shininess;
		}
		// 보통 d 아니면 Tr로 투명도 처리 (Tr = 1 - d)
		else if (Token == "d")
		{
			ISS >> Current->Opacity;
		}
		else if (Token == "Tr")
		{
			float Tr = 0.0f;
			ISS >> Tr;
			Current->Opacity = 1.0f - Tr;
		}
		/**
		 * 0 -> 조명 계산 없음
		 * 1 -> Ka + Kd
		 * 2 -> Ka + Kd + Ks (퐁 셰이더)
		 */
		else if (Token == "illum")
		{
			ISS >> Current->IllumModel;
		}
		// TextureMap - 파싱 시점에 절대 경로로 정규화
		else if (Token == "map_Kd")
		{
			Current->DiffuseTexPath = ResolveTexPath(ISS);
			Current->bHasDiffuseTexture = true;
		}
        else if (Token == "map_Ka")
        {
			Current->AmbientTexPath = ResolveTexPath(ISS);
			Current->bHasAmbientTexture = true;
        }
        else if (Token == "map_Ks")
        {
			Current->SpecularTexPath = ResolveTexPath(ISS);
			Current->bHasSpecularTexture = true;
        }
		// 범프 맵은 그레이스케일로 높이값이 저장되어 있고 추후 노말로 변환한다고 한다.
        else if (Token == "map_bump" || Token == "bump")
        {
			Current->BumpTexPath = ResolveTexPath(ISS);
			Current->bHasBumpTexture = true;
        }
    }

    return true;
}

bool FObjMtlLoader::Load(const FString& FilePath, TMap<FString, UMaterial*>& OutMaterialAssets, ID3D11Device* Device)
{
	std::ifstream File(std::filesystem::path(FPaths::ToWide(FilePath)));
	if (!File.is_open())
	{
		return false;
	}

	// 한글 경로 안전을 위해 wide string 기반으로 filesystem 연산 수행
	std::filesystem::path MtlDir = std::filesystem::path(FPaths::ToWide(FilePath)).parent_path();

	auto ResolveTexPath = [&](std::istringstream& InISS) -> FString
		{
			FString RelPath;

			InISS >> RelPath;
			if (RelPath.empty())
			{
				return {};
			}

			std::filesystem::path FileName = std::filesystem::path(FPaths::ToWide(RelPath)).filename();

			FString outTexPath = "";
			FFileUtils::FindFileRecursively(
				FPaths::ToUtf8(MtlDir.generic_wstring()),
				FPaths::ToUtf8(FileName.generic_wstring()),
				outTexPath);

			// 기존: std::filesystem::path TexPath = (MtlDir / outTexPath).lexically_normal();
			// 변경: UTF-8 문자열(outTexPath)을 wide로 명시 변환 후 결합
			std::filesystem::path TexPath = (MtlDir / std::filesystem::path(FPaths::ToWide(outTexPath))).lexically_normal();

			return FPaths::ToUtf8(TexPath.generic_wstring());
		};

	UMaterial* Current = nullptr;
	FString    Line;

	auto ParseFVector = [](std::istringstream& InISS) -> FVector
		{
			FVector Vector;
			InISS >> Vector.X >> Vector.Y >> Vector.Z;
			return Vector;
		};

	while (std::getline(File, Line))
	{
		Line = StringUtils::Trim(Line);
		if (Line.empty() || Line.front() == '#')
			continue;

		std::istringstream ISS(Line);
		FString Token;
		ISS >> Token;

		if (Token == "newmtl")
		{
			FString MatName;
			ISS >> MatName;
			OutMaterialAssets[MatName] = UObjectManager::Get().CreateObject<UMaterial>();
			Current = OutMaterialAssets[MatName];
			Current->Name = MatName;
		}
		// newmtl 이전 라인은 무시
		else if (!Current)
		{
			continue;
		}
		// 색상
		else if (Token == "Ka")
		{
			Current->MaterialData.AmbientColor = ParseFVector(ISS);
		}
		else if (Token == "Kd")
		{
			Current->MaterialData.DiffuseColor = ParseFVector(ISS);
		}
		else if (Token == "Ks")
		{
			Current->MaterialData.SpecularColor = ParseFVector(ISS);
		}
		else if (Token == "Ke")
		{
			Current->MaterialData.EmissiveColor = ParseFVector(ISS);
		}
		// 광택 집중도
		else if (Token == "Ns")
		{
			ISS >> Current->MaterialData.Shininess;
		}
		// 보통 d 아니면 Tr로 투명도 처리 (Tr = 1 - d)
		else if (Token == "d")
		{
			ISS >> Current->MaterialData.Opacity;
		}
		else if (Token == "Tr")
		{
			float Tr = 0.0f;
			ISS >> Tr;
			Current->MaterialData.Opacity = 1.0f - Tr;
		}
		/**
		 * 0 -> 조명 계산 없음
		 * 1 -> Ka + Kd
		 * 2 -> Ka + Kd + Ks (퐁 셰이더)
		 */
		else if (Token == "illum")
		{
			ISS >> Current->MaterialData.IllumModel;
		}
		// TextureMap - 파싱 시점에 절대 경로로 정규화
		else if (Token == "map_Kd")
		{
			Current->MaterialData.DiffuseTexPath = ResolveTexPath(ISS);
			Current->MaterialData.bHasDiffuseTexture = true;
		}
		else if (Token == "map_Ka")
		{
			Current->MaterialData.AmbientTexPath = ResolveTexPath(ISS);
			Current->MaterialData.bHasAmbientTexture = true;
		}
		else if (Token == "map_Ks")
		{
			Current->MaterialData.SpecularTexPath = ResolveTexPath(ISS);
			Current->MaterialData.bHasSpecularTexture = true;
		}
		// 범프 맵은 그레이스케일로 높이값이 저장되어 있고 추후 노말로 변환한다고 한다.
		else if (Token == "map_bump" || Token == "bump")
		{
			Current->MaterialData.BumpTexPath = ResolveTexPath(ISS);
			Current->MaterialData.bHasBumpTexture = true;
		}
	}

	for (auto& [Name, Mat] : OutMaterialAssets)
	{
		Mat->MaterialParams["AmbientColor"] = FMaterialParamValue(Mat->MaterialData.AmbientColor);
		Mat->MaterialParams["DiffuseColor"] = FMaterialParamValue(Mat->MaterialData.DiffuseColor);
		Mat->MaterialParams["SpecularColor"] = FMaterialParamValue(Mat->MaterialData.SpecularColor);
		Mat->MaterialParams["EmissiveColor"] = FMaterialParamValue(Mat->MaterialData.EmissiveColor);
		Mat->MaterialParams["Shininess"] = FMaterialParamValue(Mat->MaterialData.Shininess);
		Mat->MaterialParams["Opacity"] = FMaterialParamValue(Mat->MaterialData.Opacity);
		
		if (Mat->MaterialData.bHasDiffuseTexture)
			Mat->MaterialParams["DiffuseMap"] = FMaterialParamValue(FResourceManager::Get().LoadTextureAsset(Mat->MaterialData.DiffuseTexPath, Device));
		
		if (Mat->MaterialData.bHasAmbientTexture)
			Mat->MaterialParams["AmbientMap"] = FMaterialParamValue(FResourceManager::Get().LoadTextureAsset(Mat->MaterialData.AmbientTexPath, Device));
		
		if (Mat->MaterialData.bHasSpecularTexture)
			Mat->MaterialParams["SpecularMap"] = FMaterialParamValue(FResourceManager::Get().LoadTextureAsset(Mat->MaterialData.SpecularTexPath, Device));
		
		if (Mat->MaterialData.bHasBumpTexture)
			Mat->MaterialParams["BumpMap"] = FMaterialParamValue(FResourceManager::Get().LoadTextureAsset(Mat->MaterialData.BumpTexPath, Device));

		Mat->MaterialParams["bHasDiffuseMap"] = FMaterialParamValue(Mat->MaterialData.bHasDiffuseTexture);
		Mat->MaterialParams["bHasSpecularMap"] = FMaterialParamValue(Mat->MaterialData.bHasSpecularTexture);
		Mat->MaterialParams["bHasAmbientMap"] = FMaterialParamValue(Mat->MaterialData.bHasAmbientTexture);
		Mat->MaterialParams["bHasBumpMap"] = FMaterialParamValue(Mat->MaterialData.bHasBumpTexture);
	}

	return true;

}

void UMaterial::Bind(ID3D11DeviceContext* Context) const
{
	if (!Shader) return;
	Shader->Bind(Context);

	ApplyParams(Context, MaterialParams);
}

void UMaterial::ApplyParams(ID3D11DeviceContext* Context, const TMap<FString, FMaterialParamValue>& Params) const
{
	TArray<uint8> CBufferData(Shader->GetCBufferSize());

	for (const auto& [Name, ParamValue] : Params)
	{
		FShaderVariableInfo VarInfo;
		if (Shader->GetShaderVariableInfo(Name, VarInfo))
		{
			switch (ParamValue.Type)
			{
			case EMaterialParamType::Bool:
			{
				uint32 BoolVal = std::get<bool>(ParamValue.Value) ? 1 : 0;
				std::memcpy(CBufferData.data() + VarInfo.Offset, &BoolVal, sizeof(uint32));
				break;
			}
			case EMaterialParamType::Scalar:
			{
				float Val = std::get<float>(ParamValue.Value);
				std::memcpy(CBufferData.data() + VarInfo.Offset, &Val, sizeof(float));
				break;
			}
			case EMaterialParamType::Vector2:
			{
				FVector2 Val = std::get<FVector2>(ParamValue.Value);
				std::memcpy(CBufferData.data() + VarInfo.Offset, &Val, sizeof(FVector2));
				break;
			}
			case EMaterialParamType::Vector3:
			{
				FVector Val = std::get<FVector>(ParamValue.Value);
				std::memcpy(CBufferData.data() + VarInfo.Offset, &Val, sizeof(FVector));
				break;
			}
			case EMaterialParamType::Vector4:
			{
				FVector4 Val = std::get<FVector4>(ParamValue.Value);
				std::memcpy(CBufferData.data() + VarInfo.Offset, &Val, sizeof(FVector4));
				break;
			}
			case EMaterialParamType::Matrix4:
			{
				FMatrix Val = std::get<FMatrix>(ParamValue.Value);
				std::memcpy(CBufferData.data() + VarInfo.Offset, &Val, sizeof(FMatrix));
				break;
			}
			default:
				break;
			}
		}
		else
		{
			if (ParamValue.Type == EMaterialParamType::Texture && std::holds_alternative<UTexture*>(ParamValue.Value))
			{
				int32 Slot = Shader->GetTextureBindSlot(Name);
				if (Slot >= 0)
				{
					UTexture* TextureAsset = std::get<UTexture*>(ParamValue.Value);
					if (TextureAsset)
					{
						ID3D11ShaderResourceView* SRV = TextureAsset->GetSRV();
						Context->PSSetShaderResources(Slot, 1, &SRV);
					}
				}
			}
		}
	}

	Shader->UpdateAndBindCBuffer(Context, CBufferData.data(), 6, CBufferData.size());
}

void UMaterialInstance::Bind(ID3D11DeviceContext* Context) const
{
	if (!Parent) return;

	TMap<FString, FMaterialParamValue> FinalParams;

	Parent->GatherAllParams(FinalParams);

	for (const auto& [Name, Value] : OverridedParams)
	{
		FinalParams[Name] = Value;
	}

	Parent->ApplyParams(Context, FinalParams);
}
