#include "ObjLoader.h"

#include "FileUtils.h"

//	v, vt, vn, mtllib, usemtl, f
UStaticMesh* FObjLoader::Load(const FString& Path)
{
	Reset();
	
	SourcePath = Path;

	if (!ParseObj(Path))
	{
		return nullptr;
	}

	if (!BuildStaticMesh())
	{
		return nullptr;
	}

	if (!BindMaterials())
	{
		return nullptr;
	}

	return CreateAsset();
}

//	TODO : 대소문자 정규화 해야하나
bool FObjLoader::SupportsExtension(const FString& Extension) const
{
	return Extension == FString("obj") || Extension == FString(".obj") || Extension == FString("OBJ") || Extension == FString(".OBJ");
}

FString FObjLoader::GetLoaderName() const
{
	return FString{"FObjLoader"};
}

bool FObjLoader::ParseObj(const FString& Path)
{
	TArray<FString> Lines;

	if (!FFileUtils::LoadFileToLines(Path, Lines))
	{
		return false;
	}

	FString CurrentMaterialName;

	for (const auto& RawLine : Lines)
	{
		FString Line = StringUtils::Trim(RawLine);

		if (Line.empty() || StringUtils::StartWith(Line, "#"))
		{
			continue;
		}

		if (StringUtils::StartWith(Line, "v "))
		{
			if (!ParsePositionLine(Line))
			{
				return false;
			}
		}
		else if (StringUtils::StartWith(Line, "vt "))
		{
			if (!ParseTexCoordLine(Line))
			{
				return false;
			}
		}
		else if (StringUtils::StartWith(Line, "vn "))
		{
			if (!ParseNormalLine(Line))
			{
				return false;
			}
		}
		else if (StringUtils::StartWith(Line, "mtllib "))
		{
			ParseMtllibLine(Line);
		}
		else if (StringUtils::StartWith(Line, "usemtl "))
		{
			ParseUseMtlLine(Line, CurrentMaterialName);
		}
		else if (StringUtils::StartWith(Line, "f "))
		{
			if (!ParseFaceLine(Line, CurrentMaterialName))
			{
				return false;
			}
		}
	}

	return !RawData.Positions.empty() && !RawData.Faces.empty();
}

bool FObjLoader::BuildStaticMesh()
{
	//	Mesh를 생성할 Raw Data 존재 확인
	if (RawData.Positions.empty() || RawData.Faces.empty())
	{
		return false;
	}

	StaticMeshAsset.PathFileName = SourcePath;
	StaticMeshAsset.Vertices.clear();
	StaticMeshAsset.Indices.clear();
	StaticMeshAsset.Sections.clear();
	StaticMeshAsset.MaterialSlots.clear();

	//	usemtl 이름 기준으로 slot 목록 생성 (slot per usemtl)
	for (const FObjRawFace& Face : RawData.Faces)
	{
		GetOrAddMaterialSlot(Face.MaterialName);
	}

	//	IndexBuffer를 위한 Map
	TMap<FObjVertexKey, uint32> VertexMap;

	//	Slot 별로 Section 생성 (Section per Material Slot)
	for (int32 SlotIdx = 0; SlotIdx < static_cast<int32>(StaticMeshAsset.MaterialSlots.size()); SlotIdx++)
	{
		const FString& SlotName = StaticMeshAsset.MaterialSlots[SlotIdx].SlotName;

		FStaticMeshSection NewSection;
		NewSection.StartIndex = static_cast<int32>(StaticMeshAsset.Indices.size());
		NewSection.MaterialSlotIndex = SlotIdx;

		for (const FObjRawFace& Face : RawData.Faces)
		{
			FString FaceMaterialName = Face.MaterialName;

			if (FaceMaterialName.empty())
			{
				FaceMaterialName = "Default";
			}

			//	현재 slot에만 해당하는 face만 처리
			if (FaceMaterialName != SlotName)
			{
				continue;
			}
			
			//	물론 일어나지 않을 일이긴 함
			if (Face.Vertices.size() < 3)
			{
				continue;
			}
			
			for (uint32 i = 0; i < Face.Vertices.size() - 2; i++)
			{
				uint32 I0 = GetOrCreateVertexIndex(Face.Vertices[0], VertexMap);
				uint32 I1 = GetOrCreateVertexIndex(Face.Vertices[i + 1], VertexMap);
				uint32 I2 = GetOrCreateVertexIndex(Face.Vertices[i + 2], VertexMap);

				StaticMeshAsset.Indices.push_back(I0);
				StaticMeshAsset.Indices.push_back(I1);
				StaticMeshAsset.Indices.push_back(I2);
			}
		}

		//	Slot이 차지하는 index 범위를 section으로 기록
		NewSection.IndexCount = static_cast<uint32>(StaticMeshAsset.Indices.size()- NewSection.StartIndex);

		if (NewSection.IndexCount > 0)
		{
			StaticMeshAsset.Sections.push_back(NewSection);
		}
	}

	return !StaticMeshAsset.Vertices.empty() && !StaticMeshAsset.Indices.empty();
}

bool FObjLoader::BindMaterials()
{
	return false;
}

UStaticMesh* FObjLoader::CreateAsset()
{
	return nullptr;
}

void FObjLoader::Reset()
{
	SourcePath.clear();

	RawData = {};
	StaticMeshAsset = {};
}

#pragma region __HELPER__

//	v
bool FObjLoader::ParsePositionLine(const FString& Line)
{
	TArray<FString> Tokens = StringUtils::Split(Line);

	//	4개를 보장해야 함
	if (Tokens.size() < 4)
	{
		return false;
	}

	FVector Position;
	Position.X = std::stof(Tokens[1]);
	Position.Y = std::stof(Tokens[2]);
	Position.Z = std::stof(Tokens[3]);

	RawData.Positions.push_back(Position);
	return true;
}

//	vt
bool FObjLoader::ParseTexCoordLine(const FString& Line)
{
	TArray<FString> Tokens = StringUtils::Split(Line);

	//	3개를 보장해야 함
	if (Tokens.size() < 3)
	{
		return false;
	}

	FVector2 TexCoord;
	TexCoord.X = std::stof(Tokens[1]);
	TexCoord.Y = std::stof(Tokens[2]);

	RawData.UVs.push_back(TexCoord);
	return true;
}

//	vn
bool FObjLoader::ParseNormalLine(const FString& Line)
{
	TArray<FString> Tokens = StringUtils::Split(Line);

	//	4개를 보장해야 함
	if (Tokens.size() < 4)
	{
		return false;
	}

	FVector Normal;
	Normal.X = std::stof(Tokens[1]);
	Normal.Y = std::stof(Tokens[2]);
	Normal.Z = std::stof(Tokens[3]);

	RawData.Normals.push_back(Normal);
	return true;
}

//	mtllib
void FObjLoader::ParseMtllibLine(const FString& Line)
{
	TArray<FString> Tokens = StringUtils::Split(Line);

	//	파일명이 존재하는 지 여부만 확인
	if (Tokens.size() >= 2)
	{
		RawData.ReferencedMtlPath = Tokens[1];
	}
}

//	usemtl
void FObjLoader::ParseUseMtlLine(const FString& Line, FString& CurrentMaterialName)
{
	TArray<FString> Tokens = StringUtils::Split(Line);

	if (Tokens.size() >= 2)
	{
		CurrentMaterialName = Tokens[1];
	}
}


bool FObjLoader::ParseFaceLine(const FString& Line, const FString& CurrentMaterialName)
{
	TArray<FString> Tokens = StringUtils::Split(Line);

	//	surface 정보는 최소한 4개를 보장 (face는 3개가 아닐 수도 있음)
	//	이후에 triangulation 진행해야 함
	if (Tokens.size() < 4)
	{
		return false;
	}

	FObjRawFace Face;
	Face.MaterialName = CurrentMaterialName;

	for (uint32 i = 1; i < Tokens.size(); i++)
	{
		FObjRawIndex Idx;
		if (!ParseFaceVertexToken(Tokens[i], Idx))
		{
			return false;
		}

		Face.Vertices.push_back(Idx);
	}

	RawData.Faces.push_back(Face);
	return true;
}

//	Obj index는 1-based이기에 0-based로 변경
//	NOTE : Obj는 종종 negative index를 사용할 때도 있음 (그러나 지원하지 않는게 편할 듯) - 필요하면 추가할 것
bool FObjLoader::ParseFaceVertexToken(const FString& Token, FObjRawIndex& OutIndex)
{
	TArray<FString> Parts = StringUtils::Split(Token);

	if (Parts.size() >= 1 && !Parts[0].empty())
	{
		OutIndex.PositionIndex = std::stoi(Parts[0]) - 1;
	}
	if (Parts.size() >= 2 && !Parts[1].empty())
	{
		OutIndex.UVIndex = std::stoi(Parts[1]) - 1;
	}
	if (Parts.size() >= 3 && !Parts[2].empty())
	{
		OutIndex.NormalIndex = std::stoi(Parts[2]) - 1;
	}

	return OutIndex.PositionIndex >= 0;
}

int32 FObjLoader::GetOrAddMaterialSlot(const FString& MaterialName)
{
	FString SlotName = MaterialName;
	if (SlotName.empty())
	{
		SlotName = "Default";
	}

	//	이미 존재하는 MaterialSlot인지 확인
	for (int32 i = 0; i < static_cast<int32>(StaticMeshAsset.MaterialSlots.size()); i++)
	{
		if (StaticMeshAsset.MaterialSlots[i].SlotName == SlotName)
		{
			return i;
		}
	}

	FStaticMeshMaterialSlot NewSlot = {};
	NewSlot.SlotName = SlotName;
	NewSlot.DefaultMaterial = nullptr;

	StaticMeshAsset.MaterialSlots.push_back(NewSlot);
	return static_cast<int32>(StaticMeshAsset.MaterialSlots.size() - 1);
}

//	Raw Index -> 최종 Vertex 생성
FNormalVertex FObjLoader::MakeVertex(const FObjRawIndex& RawIndex) const
{
	FNormalVertex Vertex = {};

	if (RawIndex.PositionIndex >= 0 && RawIndex.PositionIndex < static_cast<int32>(RawData.Positions.size()))
	{
		Vertex.Position = RawData.Positions[RawIndex.PositionIndex];
	}
	if (RawIndex.NormalIndex >= 0 && RawIndex.NormalIndex < static_cast<int32>(RawData.Normals.size()))
	{
		Vertex.Normal = RawData.Normals[RawIndex.NormalIndex];
	}
	else
	{
		Vertex.Normal = FVector(0.0f, 0.0f, 1.0f);
	}

	//	White로 초기화
	Vertex.Color = FVector4{1.f, 1.f, 1.f, 1.f};

	if (RawIndex.UVIndex >= 0 && RawIndex.UVIndex < static_cast<int32>(RawData.UVs.size()))
	{
		Vertex.UVs = RawData.UVs[RawIndex.UVIndex];
	}
	else
	{
		Vertex.UVs = FVector2{0.0f, 0.0f};
	}

	return Vertex;
}

uint32 FObjLoader::GetOrCreateVertexIndex(const FObjRawIndex& RawIndex, TMap<FObjVertexKey, uint32>& VertexMap)
{
	FObjVertexKey Key = {};
	Key.ObjRawIndex.PositionIndex = RawIndex.PositionIndex;
	Key.ObjRawIndex.NormalIndex = RawIndex.NormalIndex;
	Key.ObjRawIndex.UVIndex = RawIndex.UVIndex;

	auto It = VertexMap.find(Key);
	if (It != VertexMap.end())
	{
		return It->second;
	}

	FNormalVertex NewVertex = MakeVertex(RawIndex);
	uint32 NewIndex = static_cast<uint32>(StaticMeshAsset.Vertices.size());
	
	StaticMeshAsset.Vertices.push_back(NewVertex);
	VertexMap.emplace(Key, NewIndex);

	return NewIndex;
}

#pragma endregion
