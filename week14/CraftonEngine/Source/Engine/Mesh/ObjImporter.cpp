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
			// default material section 추가 (usemtl이 없는 경우)
			if (ObjInfo.MaterialSections.empty())
			{
				FObjInfo::FObjMaterialSection DefaultSection;
				DefaultSection.MaterialName = "Default";
				DefaultSection.StartIndex = 0;
				DefaultSection.IndexCount = 0;
				ObjInfo.MaterialSections.emplace_back(DefaultSection);
			}

			// TODO: (v, vt, vn) 모두 존재하고 Face가 삼각형이며 양수 인덱스라고 가정
			std::string FaceVertex;
			for (int i = 0; i < 3; ++i)
			{
				LineStream >> FaceVertex;
				uint32 v, vt, vn;

				if (sscanf_s(FaceVertex.c_str(), "%u/%u/%u", &v, &vt, &vn) == 3)
				{
					ObjInfo.PosIndices.emplace_back(v - 1);
					ObjInfo.UVIndices.emplace_back(vt - 1);
					ObjInfo.NormalIndices.emplace_back(vn - 1);
				}
				else
				{
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
			if (!ObjInfo.MaterialSections.empty())
			{
				ObjInfo.MaterialSections.back().IndexCount = static_cast<uint32>(ObjInfo.PosIndices.size()) - ObjInfo.MaterialSections.back().StartIndex;
			}
			FObjInfo::FObjMaterialSection Section;
			LineStream >> Section.MaterialName;
			Section.StartIndex = static_cast<uint32>(ObjInfo.PosIndices.size());
			ObjInfo.MaterialSections.emplace_back(Section);
		}
		else if (Prefix == "o")
		{
			LineStream >> ObjInfo.ObjectName;
		}
	}

	if (!ObjInfo.MaterialSections.empty())
	{
		ObjInfo.MaterialSections.back().IndexCount = static_cast<uint32>(ObjInfo.PosIndices.size()) - ObjInfo.MaterialSections.back().StartIndex;
	}
	return ObjInfo;
}

TArray<FObjMaterialInfo> FObjImporter::ParseMtl(const std::string& MtlPath)
{
	TArray<FObjMaterialInfo> MaterialInfos;
	std::ifstream File(MtlPath);

	if (!File.is_open())
	{
		UE_LOG("Failed to open MTL file: %s", MtlPath.c_str());
		return MaterialInfos;
	}

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
		else if (Prefix == "newmtl")
		{
			FObjMaterialInfo MaterialInfo;
			LineStream >> MaterialInfo.Name;
			MaterialInfos.emplace_back(MaterialInfo);
		}
		else if (Prefix == "Kd")
		{
			if (MaterialInfos.empty())
			{
				continue;
			}
			FVector DiffuseColor;
			LineStream >> DiffuseColor.X >> DiffuseColor.Y >> DiffuseColor.Z;
			MaterialInfos.back().DiffuseColor = DiffuseColor;
		}
		else if (Prefix == "map_Kd")
		{
			if (MaterialInfos.empty())
			{
				continue;
			}
			std::string TexturePath;
			LineStream >> TexturePath;
			MaterialInfos.back().DiffuseTexture = TexturePath;
		}
	}

	return MaterialInfos;
}

// assume that all faces are triangles and that (v, vt, vn) are all present.
// and, size of PosIndices, UVIndices, NormalIndices are all the same.
// Not yet handling "usemtl" and material assignment.
FStaticMesh FObjImporter::Convert(const FObjInfo& ObjInfo)
{
	FStaticMesh OutMesh = FStaticMesh();

	// TODO: unordered_map은 FVertexKey의 해시 함수를 정의해야 해서 map으로 대체
	std::map<FVertexKey, uint32> VertexMap;

	// for each Triangle (3 indices)
	const size_t IndexCount = ObjInfo.PosIndices.size();
	for (size_t i = 0; i < IndexCount; i += 3)
	{
		// indices to be added to index buffer for this triangle
		uint32 TriangleIndices[3];

		for (int j = 0; j < 3; ++j)
		{
			size_t CurrentIndex = i + j;
			FVertexKey Key = {
				ObjInfo.PosIndices[CurrentIndex],
				ObjInfo.UVIndices[CurrentIndex],
				ObjInfo.NormalIndices[CurrentIndex]
			};

			auto It = VertexMap.find(Key);
			if (It != VertexMap.end())
			{
				// already exists, reuse vertex buffer index
				TriangleIndices[j] = It->second;
			}
			else
			{
				// new vertex, add to vertex buffer
				FNormalVertex NewVertex;

				NewVertex.pos = ObjInfo.Positions[Key.p];
				NewVertex.normal = ObjInfo.Normals[Key.n];

				// RHS -> LHS
				NewVertex.pos.Z = -NewVertex.pos.Z;
				NewVertex.normal.Z = -NewVertex.normal.Z;

				// UV left-bottom -> left-top
				NewVertex.tex = ObjInfo.UVs[Key.t];

				NewVertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };

				uint32 NewIndex = static_cast<uint32>(OutMesh.Vertices.size());
				OutMesh.Vertices.emplace_back(NewVertex);

				VertexMap[Key] = NewIndex;
				TriangleIndices[j] = NewIndex;
			}
		}

		// CCW -> CW
		OutMesh.Indices.emplace_back(TriangleIndices[0]);
		OutMesh.Indices.emplace_back(TriangleIndices[2]);
		OutMesh.Indices.emplace_back(TriangleIndices[1]);
	}
	return OutMesh;
}
