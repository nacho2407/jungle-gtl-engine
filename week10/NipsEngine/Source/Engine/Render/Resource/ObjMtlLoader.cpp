#include "ObjMtlLoader.h"
#include "Asset/FileUtils.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"

#include <filesystem>

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
			Mat->MaterialParams["DiffuseMap"] = FMaterialParamValue(FResourceManager::Get().LoadTexture(Mat->MaterialData.DiffuseTexPath, Device));

		if (Mat->MaterialData.bHasAmbientTexture)
			Mat->MaterialParams["AmbientMap"] = FMaterialParamValue(FResourceManager::Get().LoadTexture(Mat->MaterialData.AmbientTexPath, Device));

		if (Mat->MaterialData.bHasSpecularTexture)
			Mat->MaterialParams["SpecularMap"] = FMaterialParamValue(FResourceManager::Get().LoadTexture(Mat->MaterialData.SpecularTexPath, Device));

		if (Mat->MaterialData.bHasBumpTexture)
			Mat->MaterialParams["BumpMap"] = FMaterialParamValue(FResourceManager::Get().LoadTexture(Mat->MaterialData.BumpTexPath, Device));

		Mat->MaterialParams["bHasDiffuseMap"] = FMaterialParamValue(Mat->MaterialData.bHasDiffuseTexture);
		Mat->MaterialParams["bHasSpecularMap"] = FMaterialParamValue(Mat->MaterialData.bHasSpecularTexture);
		Mat->MaterialParams["bHasAmbientMap"] = FMaterialParamValue(Mat->MaterialData.bHasAmbientTexture);
		Mat->MaterialParams["bHasBumpMap"] = FMaterialParamValue(Mat->MaterialData.bHasBumpTexture);

		Mat->MaterialParams["ScrollUV"] = FMaterialParamValue(FVector2(0.0f, 0.0f));
	}

	return true;

}
