#include "Mesh/ObjImporter.h"
#include "Mesh/StaticMeshAsset.h"
#include "Editor/UI/EditorConsoleWidget.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

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
    TArray<FString> OrderedMaterialSlots;
    bool bHasNoneSlot = false;

    // OBJ의 Sections(usemtl) 등장 순서대로 고유 슬롯 수집
	for (const FStaticMeshSection& Section : ObjInfo.Sections)
    {
        const FString& CurrentSlotName = Section.MaterialSlotName;

        if (CurrentSlotName == "None")
        {
            bHasNoneSlot = true;
            continue;
        }

        if (std::find(OrderedMaterialSlots.begin(), OrderedMaterialSlots.end(), CurrentSlotName) == OrderedMaterialSlots.end())
        {
            OrderedMaterialSlots.push_back(CurrentSlotName);
        }
    }

    // 수집된 순서대로 머티리얼 생성 및 인덱스 매핑
	for (const FString& TargetSlotName : OrderedMaterialSlots)
    {
        // 슬롯 이름과 일치하는 파싱된 머티리얼 데이터 선형 탐색
        const FObjMaterialInfo* MatchedMaterial = nullptr;
		auto It = std::find_if(MtlInfos.begin(), MtlInfos.end(),
			[&TargetSlotName](const FObjMaterialInfo& Mat) {
				return Mat.MaterialSlotName == TargetSlotName;
		});

		if (It != MtlInfos.end())
		{
			MatchedMaterial = &(*It);
		}

		// 매칭된 머티리얼 정보로 FStaticMaterial 생성
		FStaticMaterial NewMaterial;
        NewMaterial.MaterialSlotName = TargetSlotName;
        // TODO: UObject 팩토리 사용하도록 수정
        NewMaterial.MaterialInterface = std::make_shared<UMaterial>();
        if (MatchedMaterial)
        {
            if (!MatchedMaterial->map_Kd.empty())
            {
                NewMaterial.MaterialInterface->DiffuseTextureFilePath = MatchedMaterial->map_Kd;
                NewMaterial.MaterialInterface->DiffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
            }
            else
            {
                NewMaterial.MaterialInterface->DiffuseColor = { MatchedMaterial->Kd.X, MatchedMaterial->Kd.Y, MatchedMaterial->Kd.Z, 1.0f };
            }
        }
        else
        {
			// 매칭 실패 시 Magenta 색상
            NewMaterial.MaterialInterface->DiffuseColor = { 1.0f, 0.0f, 1.0f, 1.0f };
        }

        OutMaterials.push_back(NewMaterial);
    }

    // "None" 슬롯이 존재했다면 맨 마지막에 배치
	if (bHasNoneSlot)
    {
        FStaticMaterial DefaultMaterial;
        DefaultMaterial.MaterialSlotName = "None";
        // TODO: UObject 팩토리 사용하도록 수정
        DefaultMaterial.MaterialInterface = std::make_shared<UMaterial>();
        DefaultMaterial.MaterialInterface->DiffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f };

        OutMaterials.push_back(DefaultMaterial);
    }

    // Phase 2: 파편화된 섹션들의 면(Face)을 머티리얼 인덱스 기준으로 재그룹화
	TArray<TArray<uint32>> FacesPerMaterial;
    FacesPerMaterial.resize(OutMaterials.size());

    for (const FStaticMeshSection& RawSection : ObjInfo.Sections)
    {
		// 섹션의 머티리얼 슬롯 이름과 일치하는 OutMaterials 배열의 인덱스 찾기
		auto It = std::find_if(OutMaterials.begin(), OutMaterials.end(),
			[&RawSection](const FStaticMaterial& Mat) {
				return Mat.MaterialSlotName == RawSection.MaterialSlotName;
        });

		size_t MaterialIndex = 0;
		if (It != OutMaterials.end())
		{
			MaterialIndex = std::distance(OutMaterials.begin(), It);
		}
		else
		{
			// "None" 슬롯이 없고 매칭되는 슬롯도 없는 경우, 기본 머티리얼로 할당
			MaterialIndex = OutMaterials.size() - 1; // "None" 슬롯이 마지막에 배치되어 있다고 가정
			UE_LOG("Warning: Material slot '%s' not found. Assigning to Default slot.", RawSection.MaterialSlotName.c_str());
		}

        for (uint32 i = 0; i < RawSection.NumTriangles; ++i)
        {
            uint32 FaceStartIndex = RawSection.FirstIndex + (i * 3);
            FacesPerMaterial[MaterialIndex].push_back(FaceStartIndex);
        }
    }

    // Phase 3: 재그룹화된 면 데이터를 기반으로 최종 정점/인덱스 버퍼 구축
    TMap<FVertexKey, uint32> VertexMap;

    for (size_t MaterialIndex = 0; MaterialIndex < OutMaterials.size(); ++MaterialIndex)
    {
        const TArray<uint32>& FaceStarts = FacesPerMaterial[MaterialIndex];
        if (FaceStarts.empty()) continue;

        FStaticMeshSection NewSection;
        NewSection.MaterialSlotName = OutMaterials[MaterialIndex].MaterialSlotName;
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

                if (auto It = VertexMap.find(Key); It != VertexMap.end())
                {
					// 이미 생성된 정점이 있다면 인덱스 재사용
                    TriangleIndices[j] = It->second;
                }
                else
                {
					// 새로운 정점 생성
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

	OutMesh.PathFileName = ObjFilePath;
	return true;
}
