#include "Material.h"
#include "Asset/FileUtils.h"

#include <filesystem>

bool FObjMtlLoader::Load(const FString& FilePath, TMap<FString, FMaterial>& OutMaterials)
{
    std::ifstream File(FilePath);
	if (!File.is_open())
	{
		return false;
	}


	std::filesystem::path MtlDir = std::filesystem::path(FilePath).parent_path();
	auto ResolveTexPath = [&](std::istringstream& InISS) -> FString
		{
			FString RelPath;
			InISS >> RelPath;
			if (RelPath.empty())
			{
				return {};
			}
			// 중복된 경로 해결
			return (MtlDir / RelPath).lexically_normal().string();
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
		}
        else if (Token == "map_Ka")
        {
			Current->AmbientTexPath = ResolveTexPath(ISS);
        }
        else if (Token == "map_Ks")
        {
			Current->SpecularTexPath = ResolveTexPath(ISS);
        }
		// 범프 맵은 그레이스케일로 높이값이 저장되어 있고 추후 노말로 변환한다고 한다.
        else if (Token == "map_bump" || Token == "bump")
        {
			Current->BumpTexPath = ResolveTexPath(ISS);
        }
    }

    return true;
}
