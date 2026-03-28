#include "Mesh/ObjImporter.h"
#include "Mesh/StaticMeshAsset.h"
#include "Editor/UI/EditorConsoleWidget.h"
#include <fstream>
#include <sstream>
#include <map>
#include <filesystem>

struct FVertexKey {
    uint32 p, t, n;
    bool operator==(const FVertexKey& Other) const {
        return p == Other.p && t == Other.t && n == Other.n;
    }
};

namespace std {
template<>
struct hash<FVertexKey>
{
    size_t operator()(const FVertexKey& Key) const noexcept
    {
        return ((size_t)Key.p) ^ (((size_t)Key.t) << 8) ^ (((size_t)Key.n) << 16);
    }
};
}

namespace
{
	// 헬퍼 1: usemtl 등장 순서를 기반으로 FStaticMaterial 배열 및 인덱스 맵 생성
	void BuildMaterialArray(
		const FObjInfo& ObjInfo,
		const TArray<FObjMaterialInfo>& MtlInfos,
		TArray<FStaticMaterial>& OutMaterials,
		std::map<FString, int32>& OutMatNameToIndex)
	{
		OutMaterials.clear();
		OutMatNameToIndex.clear();

		// MtlInfo를 빠르게 이름으로 찾기 위한 맵
		std::map<FString, const FObjMaterialInfo*> MtlInfoMap;
		for (const auto& Mtl : MtlInfos)
		{
			MtlInfoMap[Mtl.MaterialSlotName] = &Mtl;
		}

		TArray<FString> OrderedSlotNames;
		bool bHasNone = false;

		// 1. OBJ의 usemtl(Sections) 등장 순서대로 고유한 슬롯 이름 수집
		for (const FStaticMeshSection& Section : ObjInfo.Sections)
		{
			const FString& SlotName = Section.MaterialSlotName;

			// None은 나중에 맨 뒤에 붙이기 위해 따로 마킹
			if (SlotName == "None")
			{
				bHasNone = true;
				continue;
			}

			// 아직 등록되지 않은 이름이라면 순서대로 추가
			if (std::find(OrderedSlotNames.begin(), OrderedSlotNames.end(), SlotName) == OrderedSlotNames.end())
			{
				OrderedSlotNames.push_back(SlotName);
			}
		}

		// 2. 수집된 순서대로 FStaticMaterial 생성
		for (const FString& SlotName : OrderedSlotNames)
		{
			FStaticMaterial StaticMat;
			StaticMat.MaterialSlotName = SlotName;
			StaticMat.MaterialInterface = std::make_shared<FMaterial>();

			auto It = MtlInfoMap.find(SlotName);
			if (It != MtlInfoMap.end() && It->second)
			{
				const FObjMaterialInfo* Info = It->second;
				if (!Info->map_Kd.empty())
				{
					StaticMat.MaterialInterface->DiffuseTextureFilePath = Info->map_Kd;
					StaticMat.MaterialInterface->DiffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
				}
				else
				{
					StaticMat.MaterialInterface->DiffuseColor = { Info->Kd.X, Info->Kd.Y, Info->Kd.Z, 1.0f };
				}
			}
			else
			{
				// MTL에 정의되지 않은 이름인 경우 기본 백색 처리
				StaticMat.MaterialInterface->DiffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
			}

			OutMatNameToIndex[SlotName] = static_cast<int32>(OutMaterials.size());
			OutMaterials.push_back(StaticMat);
		}

		// 3. "None" 슬롯이 존재했다면 무조건 배열의 맨 마지막에 배치
		if (bHasNone)
		{
			FStaticMaterial DefaultMat;
			DefaultMat.MaterialSlotName = "None";
			DefaultMat.MaterialInterface = std::make_shared<FMaterial>();
			DefaultMat.MaterialInterface->DiffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f };

			OutMatNameToIndex["None"] = static_cast<int32>(OutMaterials.size());
			OutMaterials.push_back(DefaultMat);
		}
	}

	// 헬퍼 2: 파편화된 섹션들의 면(Face)을 머티리얼 인덱스 기준으로 재그룹화
	void GroupFacesByMaterial(
		const FObjInfo& ObjInfo,
		const std::map<FString, int32>& MatNameToIndex,
		size_t NumMaterials,
		TArray<TArray<uint32>>& OutFacesPerMaterial)
	{
		OutFacesPerMaterial.resize(NumMaterials);

		for (const FStaticMeshSection& RawSection : ObjInfo.Sections)
		{
			int32 MatIndex = MatNameToIndex.at(RawSection.MaterialSlotName);
			for (uint32 i = 0; i < RawSection.NumTriangles; ++i)
			{
				uint32 FaceStartIndex = RawSection.FirstIndex + (i * 3);
				OutFacesPerMaterial[MatIndex].push_back(FaceStartIndex);
			}
		}
	}

	// 헬퍼 3: 버텍스 중복 제거, 좌표계 변환 및 최종 인덱스 버퍼 구성
	void ProcessVerticesAndSections(
		const FObjInfo& ObjInfo,
		const TArray<FStaticMaterial>& Materials,
		const TArray<TArray<uint32>>& FacesPerMaterial,
		FStaticMesh& OutMesh)
	{
		std::unordered_map<FVertexKey, uint32> VertexMap;

		for (size_t MatIndex = 0; MatIndex < Materials.size(); ++MatIndex)
		{
			const TArray<uint32>& FaceStarts = FacesPerMaterial[MatIndex];
			if (FaceStarts.empty()) continue;

			FStaticMeshSection NewSection;
			NewSection.MaterialSlotName = Materials[MatIndex].MaterialSlotName;
			NewSection.FirstIndex = static_cast<uint32>(OutMesh.Indices.size());
			NewSection.NumTriangles = static_cast<uint32>(FaceStarts.size());

			for (uint32 FaceStartIndex : FaceStarts)
			{
				uint32 TriangleIndices[3];

				for (int j = 0; j < 3; ++j)
				{
					size_t CurrentIndex = FaceStartIndex + j;
					FVertexKey Key = {
						ObjInfo.PosIndices[CurrentIndex],
						ObjInfo.UVIndices[CurrentIndex],
						ObjInfo.NormalIndices[CurrentIndex]
					};

					auto It = VertexMap.find(Key);
					if (It != VertexMap.end())
					{
						TriangleIndices[j] = It->second;
					}
					else
					{
						FNormalVertex NewVertex;
						NewVertex.pos = ObjInfo.Positions[Key.p];
						NewVertex.normal = ObjInfo.Normals[Key.n];

						// RHS -> LHS
						NewVertex.pos.Z = -NewVertex.pos.Z;
						NewVertex.normal.Z = -NewVertex.normal.Z;

						// UV left-bottom -> left-top
						NewVertex.tex = ObjInfo.UVs[Key.t];
						NewVertex.tex.V = 1.0f - NewVertex.tex.V;

						NewVertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };

						uint32 NewIndex = static_cast<uint32>(OutMesh.Vertices.size());
						OutMesh.Vertices.push_back(NewVertex);

						VertexMap[Key] = NewIndex;
						TriangleIndices[j] = NewIndex;
					}
				}

				// CCW -> CW
				OutMesh.Indices.push_back(TriangleIndices[0]);
				OutMesh.Indices.push_back(TriangleIndices[2]);
				OutMesh.Indices.push_back(TriangleIndices[1]);
			}

			OutMesh.Sections.push_back(NewSection);
		}
	}
}

bool FObjImporter::ParseObj(const FString& ObjFilePath, FObjInfo& OutObjInfo)
{
	OutObjInfo = FObjInfo();
	// TODO: 파일을 미리 탐색해서 reserve로 용량을 할당하고 파싱 시작하기
	std::ifstream File(ObjFilePath);

	if (!File.is_open())
	{
		UE_LOG("Failed to open OBJ file: %s", ObjFilePath.c_str());
		return false;
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
			OutObjInfo.Positions.emplace_back(Position);
		}
		else if (Prefix == "vt")
		{
			FVector2 UV;
			LineStream >> UV.U >> UV.V;
			OutObjInfo.UVs.emplace_back(UV);
		}
		else if (Prefix == "vn")
		{
			FVector Normal;
			LineStream >> Normal.X >> Normal.Y >> Normal.Z;
			OutObjInfo.Normals.emplace_back(Normal);
		}
		else if (Prefix == "f")
		{
			// default material section 추가 (usemtl이 없는 경우)
			if (OutObjInfo.Sections.empty())
			{
				FStaticMeshSection DefaultSection;
				DefaultSection.MaterialSlotName = "None";
				DefaultSection.FirstIndex = 0;
				DefaultSection.NumTriangles = 0;
				OutObjInfo.Sections.emplace_back(DefaultSection);
			}

			std::string FaceVertex;
			for (int i = 0; i < 3; ++i)
			{
				LineStream >> FaceVertex;
				uint32 v, vt, vn;

				if (sscanf_s(FaceVertex.c_str(), "%u/%u/%u", &v, &vt, &vn) == 3)
				{
					OutObjInfo.PosIndices.emplace_back(v - 1);
					OutObjInfo.UVIndices.emplace_back(vt - 1);
					OutObjInfo.NormalIndices.emplace_back(vn - 1);
				}
				else
				{
					UE_LOG("Failed to parse face vertex: %s", FaceVertex.c_str());
					return false;
				}
			}
		}
		else if (Prefix == "mtllib")
		{
			std::string MtlFileName;
			std::getline(LineStream >> std::ws, MtlFileName);
			std::filesystem::path FilePath(ObjFilePath);
			OutObjInfo.MaterialLibraryFilePath = (FilePath.parent_path() / MtlFileName).string();
		}
		else if (Prefix == "usemtl")
		{
			if (!OutObjInfo.Sections.empty())
			{
				OutObjInfo.Sections.back().NumTriangles = (static_cast<uint32>(OutObjInfo.PosIndices.size()) - OutObjInfo.Sections.back().FirstIndex) / 3;
			}
			FStaticMeshSection Section;
			LineStream >> Section.MaterialSlotName;
			if (Section.MaterialSlotName.empty())
			{
				Section.MaterialSlotName = "None";
			}
			Section.FirstIndex = static_cast<uint32>(OutObjInfo.PosIndices.size());
			OutObjInfo.Sections.emplace_back(Section);
		}
		else if (Prefix == "o")
		{
			LineStream >> OutObjInfo.ObjectName;
		}
	}

	if (!OutObjInfo.Sections.empty())
	{
		OutObjInfo.Sections.back().NumTriangles = (static_cast<uint32>(OutObjInfo.PosIndices.size()) - OutObjInfo.Sections.back().FirstIndex) / 3;
	}

	return true;
}

bool FObjImporter::ParseMtl(const FString& MtlFilePath, TArray<FObjMaterialInfo>& OutMtlInfos)
{
	OutMtlInfos.clear();
	std::ifstream File(MtlFilePath);

	if (!File.is_open())
	{
		UE_LOG("Failed to open MTL file: %s", MtlFilePath.c_str());
		return false;
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
			LineStream >> MaterialInfo.MaterialSlotName;
			MaterialInfo.Kd = { 1.0f, 1.0f, 1.0f }; // default diffuse color
			OutMtlInfos.emplace_back(MaterialInfo);
		}
		else if (Prefix == "Kd")
		{
			if (OutMtlInfos.empty())
			{
				continue;
			}
			LineStream >> OutMtlInfos.back().Kd.X >> OutMtlInfos.back().Kd.Y >> OutMtlInfos.back().Kd.Z;
		}
		else if (Prefix == "map_Kd")
		{
			if (OutMtlInfos.empty())
			{
				continue;
			}
			std::string TextureFileName;
			std::getline(LineStream >> std::ws, TextureFileName);
			std::filesystem::path FilePath(MtlFilePath);
			OutMtlInfos.back().map_Kd = (FilePath.parent_path() / TextureFileName).string();
		}
	}
	return true;
}

// assume that all faces are triangles and that (v, vt, vn) are all present.
// and, size of PosIndices, UVIndices, NormalIndices are all the same.
bool FObjImporter::Convert(const FObjInfo& ObjInfo, const TArray<FObjMaterialInfo>& MtlInfos, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
	OutMesh = FStaticMesh();
	OutMaterials.clear();

	std::map<FString, int32> MatNameToIndex;
	TArray<TArray<uint32>> FacesPerMaterial;

	// 1. usemtl 순서대로 재질 배열 구성 (None은 맨 뒤로)
	BuildMaterialArray(ObjInfo, MtlInfos, OutMaterials, MatNameToIndex);

	// 2. 머티리얼별로 면(Triangles) 재그룹화
	GroupFacesByMaterial(ObjInfo, MatNameToIndex, OutMaterials.size(), FacesPerMaterial);

	// 3. 버텍스 생성 및 인덱스 버퍼 구성
	ProcessVerticesAndSections(ObjInfo, OutMaterials, FacesPerMaterial, OutMesh);

	OutMesh.PathFileName = ObjInfo.ObjectName;
	return true;
}

bool FObjImporter::Import(const FString& ObjFilePath, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
	OutMaterials.clear();

	FObjInfo ObjInfo;
	if (!FObjImporter::ParseObj(ObjFilePath, ObjInfo))
	{
		UE_LOG("ParseObj failed for: %s", ObjFilePath.c_str());
		return false;
	}

	TArray<FObjMaterialInfo> ParsedMtlInfos;
	if (!ObjInfo.MaterialLibraryFilePath.empty()) {
		if (!FObjImporter::ParseMtl(ObjInfo.MaterialLibraryFilePath, ParsedMtlInfos))
		{
			UE_LOG("ParseMtl failed for: %s", ObjInfo.MaterialLibraryFilePath.c_str());
			return false;
		}
	}

	if (!FObjImporter::Convert(ObjInfo, ParsedMtlInfos, OutMesh, OutMaterials)){
		UE_LOG("Convert failed for: %s", ObjFilePath.c_str());
		return false;
	}

	return true;
}
