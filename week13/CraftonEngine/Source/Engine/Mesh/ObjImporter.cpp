#include "Mesh/ObjImporter.h"
#include "Mesh/StaticMeshAsset.h"
#include "Materials/Material.h"
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

			// face의 모든 정점을 먼저 수집
			struct FFaceVert { uint32 p, t, n; };
			TArray<FFaceVert> FaceVerts;

			std::string FaceVertex;
			while (LineStream >> FaceVertex)
			{
				uint32 v, vt, vn;
				if (sscanf_s(FaceVertex.c_str(), "%u/%u/%u", &v, &vt, &vn) == 3)
				{
					FaceVerts.push_back({ v - 1, vt - 1, vn - 1 });
				}
				else if (sscanf_s(FaceVertex.c_str(), "%u//%u", &v, &vn) == 2)
				{
					FaceVerts.push_back({ v - 1, 0, vn - 1 });
				}
				else
				{
					UE_LOG("Failed to parse face vertex: %s", FaceVertex.c_str());
					return false;
				}
			}

			if (FaceVerts.size() < 3)
			{
				UE_LOG("Face with less than 3 vertices");
				return false;
			}

			// Fan triangulation: N각형 → (N-2)개 삼각형
			for (size_t i = 1; i + 1 < FaceVerts.size(); ++i)
			{
				// 삼각형: [0, i, i+1]
				const FFaceVert* Tri[3] = { &FaceVerts[0], &FaceVerts[i], &FaceVerts[i + 1] };
				for (int j = 0; j < 3; ++j)
				{
					OutObjInfo.PosIndices.emplace_back(Tri[j]->p);
					OutObjInfo.UVIndices.emplace_back(Tri[j]->t);
					OutObjInfo.NormalIndices.emplace_back(Tri[j]->n);
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

	// UV가 하나도 없는 OBJ 파일 대비: 더미 UV 삽입
	if (OutObjInfo.UVs.empty())
	{
		OutObjInfo.UVs.emplace_back(FVector2{ 0.0f, 0.0f });
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
			// 옵션(-s, -o, -bm 등)을 스킵하고 마지막 파일 경로만 추출
			std::string TextureFileName;
			std::string Token;
			while (LineStream >> Token)
			{
				if (Token[0] == '-')
				{
					// 옵션 플래그 뒤의 인자값들 스킵 (-s x y z, -o x y z, -bm val 등)
					std::string Arg;
					while (LineStream.peek() != EOF && LineStream.peek() != '-')
					{
						size_t Pos = LineStream.tellg();
						LineStream >> Arg;
						// 숫자가 아니면 파일명 시작 → 되돌리고 break
						bool bIsNumber = !Arg.empty() && (isdigit(Arg[0]) || Arg[0] == '.' || (Arg[0] == '-' && Arg.size() > 1));
						if (!bIsNumber)
						{
							TextureFileName = Arg;
							// 파일명에 공백이 있을 수 있으므로 나머지도 붙임
							std::string Rest;
							if (std::getline(LineStream >> std::ws, Rest) && !Rest.empty())
							{
								TextureFileName += " " + Rest;
							}
							goto done;
						}
					}
				}
				else
				{
					// 옵션이 아닌 토큰 → 파일명 시작
					TextureFileName = Token;
					std::string Rest;
					if (std::getline(LineStream >> std::ws, Rest) && !Rest.empty())
					{
						TextureFileName += " " + Rest;
					}
					goto done;
				}
			}
			done:
			if (!TextureFileName.empty())
			{
				std::filesystem::path FilePath(MtlFilePath);
				OutMtlInfos.back().map_Kd = (FilePath.parent_path() / TextureFileName).string();
			}
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

    // Phase 1: usemtl 등장 순서를 기반으로 FStaticMaterial 배열 및 인덱스 맵 생성
    std::map<FString, int32> MaterialSlotNameToIndex;
    std::map<FString, const FObjMaterialInfo*> MatelialInfoMap;

    // MtlInfo를 빠르게 찾기 위한 맵 구성
    for (const auto& Mtl : MtlInfos)
    {
        MatelialInfoMap[Mtl.MaterialSlotName] = &Mtl;
    }

    TArray<FString> OrderedSlotNames;
    bool bHasNone = false;

    // OBJ의 Sections(usemtl) 등장 순서대로 머티리얼 슬롯 이름 수집
    for (const FStaticMeshSection& Section : ObjInfo.Sections)
    {
        if (Section.MaterialSlotName == "None")
        {
            bHasNone = true;
            continue;
        }

        if (std::find(OrderedSlotNames.begin(), OrderedSlotNames.end(), Section.MaterialSlotName) == OrderedSlotNames.end())
        {
            OrderedSlotNames.push_back(Section.MaterialSlotName);
        }
    }

    // 수집된 순서대로 머티리얼 초기화
    for (const FString& SlotName : OrderedSlotNames)
    {
        FStaticMaterial StaticMaterial;
        StaticMaterial.MaterialSlotName = SlotName;

		UMaterial* NewMaterial = UObjectManager::Get().CreateObject<UMaterial>();
		StaticMaterial.MaterialInterface = std::shared_ptr<UMaterial>(NewMaterial);

		NewMaterial->PathFileName = SlotName;

        auto It = MatelialInfoMap.find(SlotName);
        if (It != MatelialInfoMap.end() && It->second)
        {
            const FObjMaterialInfo* Info = It->second;
            if (!Info->map_Kd.empty())
            {
                StaticMaterial.MaterialInterface->DiffuseTextureFilePath = Info->map_Kd;
                StaticMaterial.MaterialInterface->DiffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
            }
            else
            {
                StaticMaterial.MaterialInterface->DiffuseColor = { Info->Kd.X, Info->Kd.Y, Info->Kd.Z, 1.0f };
            }
        }
        else
        {
            StaticMaterial.MaterialInterface->DiffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // MTL 누락 시 기본 백색
        }

        MaterialSlotNameToIndex[SlotName] = static_cast<int32>(OutMaterials.size());
        OutMaterials.push_back(StaticMaterial);
    }

    // "None" 슬롯이 존재했다면 맨 마지막에 배치
    if (bHasNone)
    {
        FStaticMaterial DefaultMat;
        DefaultMat.MaterialSlotName = "None";
        // TODO: UObject 팩토리 사용하도록 수정
        DefaultMat.MaterialInterface = std::make_shared<UMaterial>();
        DefaultMat.MaterialInterface->DiffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f };

        MaterialSlotNameToIndex["None"] = static_cast<int32>(OutMaterials.size());
        OutMaterials.push_back(DefaultMat);
    }

    // =========================================================================
    // Phase 2: 파편화된 섹션들의 면(Face)을 머티리얼 인덱스 기준으로 재그룹화
    // =========================================================================
    TArray<TArray<uint32>> FacesPerMaterial;
    FacesPerMaterial.resize(OutMaterials.size());

    for (const FStaticMeshSection& RawSection : ObjInfo.Sections)
    {
        int32 MatIndex = MaterialSlotNameToIndex.at(RawSection.MaterialSlotName);
        for (uint32 i = 0; i < RawSection.NumTriangles; ++i)
        {
            uint32 FaceStartIndex = RawSection.FirstIndex + (i * 3);
            FacesPerMaterial[MatIndex].push_back(FaceStartIndex);
        }
    }

    // =========================================================================
    // Phase 3: 버텍스 중복 제거, 좌표계 변환 및 최종 인덱스 버퍼 구성
    // =========================================================================
    std::unordered_map<FVertexKey, uint32> VertexMap;

    for (size_t MatIndex = 0; MatIndex < OutMaterials.size(); ++MatIndex)
    {
        const TArray<uint32>& FaceStarts = FacesPerMaterial[MatIndex];
        if (FaceStarts.empty()) continue;

        FStaticMeshSection NewSection;
        NewSection.MaterialSlotName = OutMaterials[MatIndex].MaterialSlotName;
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

                    // RHS -> LHS 변환
                    NewVertex.pos.Z = -NewVertex.pos.Z;
                    NewVertex.normal.Z = -NewVertex.normal.Z;

                    // UV 변환 (left-bottom -> left-top)
                    NewVertex.tex = ObjInfo.UVs[Key.t];
                    NewVertex.tex.V = 1.0f - NewVertex.tex.V;

                    NewVertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };

                    uint32 NewIndex = static_cast<uint32>(OutMesh.Vertices.size());
                    OutMesh.Vertices.push_back(NewVertex);

                    VertexMap[Key] = NewIndex;
                    TriangleIndices[j] = NewIndex;
                }
            }

            // CCW -> CW (인덱스 순서 변경)
            OutMesh.Indices.push_back(TriangleIndices[0]);
            OutMesh.Indices.push_back(TriangleIndices[2]);
            OutMesh.Indices.push_back(TriangleIndices[1]);
        }

        OutMesh.Sections.push_back(NewSection);
    }

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
