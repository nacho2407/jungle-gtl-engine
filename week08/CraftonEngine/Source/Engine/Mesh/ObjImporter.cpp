#include "Mesh/ObjImporter.h"
#include "Mesh/StaticMeshAsset.h"
#include "Editor/UI/EditorConsoleWidget.h"
#include <fstream>
#include <sstream>
#include <charconv>
#include <map>

struct FVertexKey
{
	uint32 p, t, n;
	bool operator<(const FVertexKey& Other) const
	{
		if (p != Other.p) return p < Other.p;
		if (t != Other.t) return t < Other.t;
		return n < Other.n;
	}
};

FObjInfo FObjImporter::ParseObj(const std::string& FilePath)
{
	FObjInfo ObjInfo;
	// TODO: 파일을 미리 탐색해서 reserve로 용량을 할당하고 파싱 시작하기
	std::ifstream File(FilePath);

	if (!File.is_open())
	{
		UE_LOG("Failed to open OBJ file: %s", FilePath.c_str());
		return ObjInfo;
	}

	FObjInfo::FObjMaterialSection CurrentSection = {};
	bool bInSection = false;

	std::string Line;
	while (std::getline(File, Line))
	{
		std::stringstream LineStream(Line);
		std::string Prefix;
		LineStream >> Prefix;

		if (Prefix.empty() || Prefix[0] == '#')
		{
			continue;
		}
		else if (Prefix == "v")
		{
			FVector Position;
			LineStream >> Position.X >> Position.Y >> Position.Z;
			ObjInfo.Positions.emplace_back(Position);
		}
		else if (Prefix == "vt")
		{
			FVector2 UV;
			LineStream >> UV.U >> UV.V;
			ObjInfo.UVs.emplace_back(UV);
		}
		else if (Prefix == "vn")
		{
			FVector Normal;
			LineStream >> Normal.X >> Normal.Y >> Normal.Z;
			ObjInfo.Normals.emplace_back(Normal);
		}
		else if (Prefix == "f")
		{
			// TODO: (v, vt, vn) 모두 존재하고 Face가 삼각형이며 양수 인덱스라고 가정
			std::string FaceVertex;
			for (int i = 0; i < 3; ++i)
			{
				LineStream >> FaceVertex;
				const char* Data = FaceVertex.data();
				const char* End = Data + FaceVertex.size();
				uint32_t v, vt, vn;

				auto Res1 = std::from_chars(Data, End, v);
				if (Res1.ec == std::errc() && Res1.ptr < End && *Res1.ptr == '/') {
					auto Res2 = std::from_chars(Res1.ptr + 1, End, vt);
					if (Res2.ec == std::errc() && Res2.ptr < End && *Res2.ptr == '/') {
						auto Res3 = std::from_chars(Res2.ptr + 1, End, vn);
						if (Res3.ec != std::errc()) {
						}
						ObjInfo.PosIndices.emplace_back(v - 1);
						ObjInfo.UVIndices.emplace_back(vt - 1);
						ObjInfo.NormalIndices.emplace_back(vn - 1);
					}
					else {
						UE_LOG("Failed to parse face vertex: %s", FaceVertex.c_str());
						return ObjInfo;
					}
				}
				else {
					UE_LOG("Failed to parse face vertex: %s", FaceVertex.c_str());
					return ObjInfo;
				}
			}
		}
		else if (Prefix == "mtllib")
		{
			LineStream >> ObjInfo.MaterialLibrary;
		}
		else if (Prefix == "usemtl")
		{
			if (bInSection)
			{
				CurrentSection.IndexCount = static_cast<uint32>(ObjInfo.PosIndices.size()) - CurrentSection.StartIndex;
				ObjInfo.MaterialSections.emplace_back(CurrentSection);
				CurrentSection = {};
				bInSection = false;
			}
			LineStream >> CurrentSection.MaterialName;
			CurrentSection.StartIndex = static_cast<uint32>(ObjInfo.PosIndices.size());
			CurrentSection.IndexCount = 0;
			bInSection = true;
		}
		else if (Prefix == "o")
		{
			LineStream >> ObjInfo.ObjectName;
		}
	}

	if (bInSection)
	{
		CurrentSection.IndexCount = static_cast<uint32>(ObjInfo.PosIndices.size()) - CurrentSection.StartIndex;
		ObjInfo.MaterialSections.emplace_back(CurrentSection);
	}

	return ObjInfo;
}

TArray<FObjMaterialInfo> FObjImporter::ParseMtl(const std::string& MtlPath)
{
	return {};
}

FStaticMesh FObjImporter::Convert(const FObjInfo& ObjInfo)
{
	return nullptr;
}
