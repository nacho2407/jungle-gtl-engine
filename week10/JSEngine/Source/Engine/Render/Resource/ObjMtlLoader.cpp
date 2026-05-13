#include "ObjMtlLoader.h"
#include "Asset/FileUtils.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"

#include <filesystem>
#include <sstream>
#include <vector>

static std::filesystem::path MakeAbsoluteEnginePath(const FString& FilePath)
{
	std::filesystem::path Path(FPaths::ToWide(FilePath));
	if (Path.is_relative())
	{
		Path = std::filesystem::path(FPaths::RootDir()) / Path;
	}

	return Path.lexically_normal();
}

static FString ToEngineRelativePath(const std::filesystem::path& Path)
{
	std::error_code Ec;
	std::filesystem::path RelativePath = std::filesystem::relative(Path, std::filesystem::path(FPaths::RootDir()), Ec);
	if (Ec)
	{
		return FPaths::ToUtf8(Path.lexically_normal().generic_wstring());
	}

	return FPaths::ToUtf8(RelativePath.lexically_normal().generic_wstring());
}

static FString ResolveMtlTexturePath(const std::filesystem::path& MtlDir, const FString& RawTexturePath)
{
	const FString TrimmedTexturePath = StringUtils::Trim(RawTexturePath);
	if (TrimmedTexturePath.empty())
	{
		return {};
	}

	std::filesystem::path TexturePath(FPaths::ToWide(TrimmedTexturePath));
	if (TexturePath.is_relative())
	{
		TexturePath = MtlDir / TexturePath;
	}

	TexturePath = TexturePath.lexically_normal();
	if (std::filesystem::exists(TexturePath) && std::filesystem::is_regular_file(TexturePath))
	{
		return ToEngineRelativePath(TexturePath);
	}

	std::filesystem::path FileName = std::filesystem::path(FPaths::ToWide(TrimmedTexturePath)).filename();
	if (FileName.empty())
	{
		return {};
	}

	FString FoundPath;
	if (FFileUtils::FindFileRecursively(
		FPaths::ToUtf8(MtlDir.generic_wstring()),
		FPaths::ToUtf8(FileName.generic_wstring()),
		FoundPath))
	{
		std::filesystem::path FoundTexturePath = (MtlDir / std::filesystem::path(FPaths::ToWide(FoundPath))).lexically_normal();
		return ToEngineRelativePath(FoundTexturePath);
	}

	return {};
}

static FString JoinTokens(const std::vector<FString>& Tokens, size_t StartIndex)
{
	FString Result;
	for (size_t Index = StartIndex; Index < Tokens.size(); ++Index)
	{
		if (!Result.empty())
		{
			Result += " ";
		}
		Result += Tokens[Index];
	}

	return Result;
}

static FString ResolveMtlTextureExpression(const std::filesystem::path& MtlDir, const FString& RawExpression)
{
	const FString TrimmedExpression = StringUtils::Trim(RawExpression);
	if (TrimmedExpression.empty())
	{
		return {};
	}

	FString TexturePath = ResolveMtlTexturePath(MtlDir, TrimmedExpression);
	if (!TexturePath.empty())
	{
		return TexturePath;
	}

	std::istringstream TokenStream(TrimmedExpression);
	std::vector<FString> Tokens;
	FString Token;
	while (TokenStream >> Token)
	{
		Tokens.push_back(Token);
	}

	for (size_t ReverseIndex = Tokens.size(); ReverseIndex > 0; --ReverseIndex)
	{
		TexturePath = ResolveMtlTexturePath(MtlDir, JoinTokens(Tokens, ReverseIndex - 1));
		if (!TexturePath.empty())
		{
			return TexturePath;
		}
	}

	return {};
}

bool FObjMtlLoader::Load(const FString& FilePath, TMap<FString, UMaterial*>& OutMaterialAssets, ID3D11Device* Device, TArray<FString>* OutMaterialOrder)
{
	const std::filesystem::path MtlPath = MakeAbsoluteEnginePath(FilePath);
	std::ifstream File(MtlPath);
	if (!File.is_open())
	{
		return false;
	}

	std::filesystem::path MtlDir = MtlPath.parent_path();

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
			Current->ImportedName = MatName;
			if (OutMaterialOrder)
			{
				OutMaterialOrder->push_back(MatName);
			}
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
			FString TextureExpression;
			std::getline(ISS, TextureExpression);
			Current->MaterialData.DiffuseTexPath = ResolveMtlTextureExpression(MtlDir, TextureExpression);
			Current->MaterialData.bHasDiffuseTexture = !Current->MaterialData.DiffuseTexPath.empty();
		}
		else if (Token == "map_Ka")
		{
			FString TextureExpression;
			std::getline(ISS, TextureExpression);
			Current->MaterialData.AmbientTexPath = ResolveMtlTextureExpression(MtlDir, TextureExpression);
			Current->MaterialData.bHasAmbientTexture = !Current->MaterialData.AmbientTexPath.empty();
		}
		else if (Token == "map_Ks")
		{
			FString TextureExpression;
			std::getline(ISS, TextureExpression);
			Current->MaterialData.SpecularTexPath = ResolveMtlTextureExpression(MtlDir, TextureExpression);
			Current->MaterialData.bHasSpecularTexture = !Current->MaterialData.SpecularTexPath.empty();
		}
		else if (Token == "map_Ke" || Token == "map_emissive" || Token == "map_Emissive")
		{
			FString TextureExpression;
			std::getline(ISS, TextureExpression);
			Current->MaterialData.EmissiveTexPath = ResolveMtlTextureExpression(MtlDir, TextureExpression);
			Current->MaterialData.bHasEmissiveTexture = !Current->MaterialData.EmissiveTexPath.empty();
		}
		// map_bump / map_Bump / bump — skip any -option value pairs before the filename
		else if (Token == "map_bump" || Token == "map_Bump" || Token == "bump")
		{
			FString TextureExpression;
			std::getline(ISS, TextureExpression);
			Current->MaterialData.BumpTexPath = ResolveMtlTextureExpression(MtlDir, TextureExpression);
			Current->MaterialData.bHasBumpTexture = !Current->MaterialData.BumpTexPath.empty();
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

		UTexture* DefaultWhite = FResourceManager::Get().GetTexture("DefaultWhite");

		if (Mat->MaterialData.bHasDiffuseTexture)
			Mat->MaterialParams["DiffuseMap"] = FMaterialParamValue(FResourceManager::Get().LoadTexture(Mat->MaterialData.DiffuseTexPath, Device));
		else
			Mat->MaterialParams["DiffuseMap"] = FMaterialParamValue(DefaultWhite);

		if (Mat->MaterialData.bHasAmbientTexture)
			Mat->MaterialParams["AmbientMap"] = FMaterialParamValue(FResourceManager::Get().LoadTexture(Mat->MaterialData.AmbientTexPath, Device));
		else
			Mat->MaterialParams["AmbientMap"] = FMaterialParamValue(DefaultWhite);

		if (Mat->MaterialData.bHasSpecularTexture)
			Mat->MaterialParams["SpecularMap"] = FMaterialParamValue(FResourceManager::Get().LoadTexture(Mat->MaterialData.SpecularTexPath, Device));
		else
			Mat->MaterialParams["SpecularMap"] = FMaterialParamValue(DefaultWhite);

		if (Mat->MaterialData.bHasEmissiveTexture)
			Mat->MaterialParams["EmissiveMap"] = FMaterialParamValue(FResourceManager::Get().LoadTexture(Mat->MaterialData.EmissiveTexPath, Device));
		else
			Mat->MaterialParams["EmissiveMap"] = FMaterialParamValue(DefaultWhite);

		if (Mat->MaterialData.bHasBumpTexture)
			Mat->MaterialParams["BumpMap"] = FMaterialParamValue(FResourceManager::Get().LoadTexture(Mat->MaterialData.BumpTexPath, Device));
		else
			Mat->MaterialParams["BumpMap"] = FMaterialParamValue(DefaultWhite);

		Mat->MaterialParams["bHasDiffuseMap"] = FMaterialParamValue(Mat->MaterialData.bHasDiffuseTexture);
		Mat->MaterialParams["bHasSpecularMap"] = FMaterialParamValue(Mat->MaterialData.bHasSpecularTexture);
		Mat->MaterialParams["bHasAmbientMap"] = FMaterialParamValue(Mat->MaterialData.bHasAmbientTexture);
		Mat->MaterialParams["bHasEmissiveMap"] = FMaterialParamValue(Mat->MaterialData.bHasEmissiveTexture);
		Mat->MaterialParams["bHasBumpMap"] = FMaterialParamValue(Mat->MaterialData.bHasBumpTexture);

		Mat->MaterialParams["ScrollUV"] = FMaterialParamValue(FVector2(0.0f, 0.0f));
	}

	return true;

}
