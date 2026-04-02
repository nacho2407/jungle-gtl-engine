#include "ObjLoader.h"
#include "FileUtils.h"
#include "Math/Utils.h"
#include "UI/EditorConsoleWidget.h"
#include "Core/PlatformTime.h"

#include <algorithm>
#include <cfloat>

//	v, vt, vn, mtllib, usemtl, f
FStaticMesh* FObjLoader::Load(const FString& Path, const FStaticMeshLoadOptions& LoadOptions)
{
	Reset();

	SourcePath = Path;

	const double StartTime = FPlatformTime::Seconds();
	UE_LOG("[ObjLoader] Start loading OBJ: %s", Path.c_str());

	/* Obj Parse - Build Raw Data */
	if (!ParseObj(Path))
	{
		UE_LOG("[ObjLoader] Failed to parse OBJ: %s", Path.c_str());
		return nullptr;
	}

	
	/* 단위 큐브의 크기로 변경 및 AABB 기준 가운데로 고정 */
	if (LoadOptions.bNormalizeToUnitCube)
	{
		UE_LOG("[ObjLoader] NormalizeToUnitCube enabled: %s", Path.c_str());
		
		NormalizeRawPositionsToUnitCube();
	}
	else
	{
		/* 중점 좌표를 AABB 기준 가운데로 고정 */
		NormalizeRawPositionsToUnitCube();
	}
	
	/* Build Cooked Data from Raw Data */
	if (!BuildStaticMesh())
	{
		UE_LOG("[ObjLoader] Failed to build static mesh: %s", Path.c_str());
		return nullptr;
	}
	UE_LOG("[ObjLoader] OBJ Loaded: %s (Vertices: %zu, Indices: %zu, Sections: %zu, Slots: %zu)",
		SourcePath.c_str(),
		StaticMeshAsset.Vertices.size(),
		StaticMeshAsset.Indices.size(),
		StaticMeshAsset.Sections.size(),
		BuiltMaterialSlotName.size());
		// ,StaticMeshAsset.MaterialSlots.size());

	const double EndTime = FPlatformTime::Seconds();
	UE_LOG("[ObjLoader] Loaded %s in %.3f sec", Path.c_str(), EndTime - StartTime);

	return new FStaticMesh(StaticMeshAsset);
}

//	TODO : 나중에 다시 확인해보기
bool FObjLoader::SupportsExtension(const FString& Extension) const
{
	return Extension == FString("obj") || Extension == FString(".obj") || Extension == FString("OBJ") || Extension == FString(".OBJ");
}

FString FObjLoader::GetLoaderName() const
{
	return FString{ "FObjLoader" };
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
	// Mesh를 생성할 Raw Data 존재 확인
	if (RawData.Positions.empty() || RawData.Faces.empty())
	{
		return false;
	}

	StaticMeshAsset.PathFileName = SourcePath;
	StaticMeshAsset.Vertices.clear();
	StaticMeshAsset.Indices.clear();
	StaticMeshAsset.Sections.clear();
	StaticMeshAsset.SlotNames.clear();
	BuiltMaterialSlotName.clear();

	// usemtl 이름 기준으로 slot 목록 생성 
	// for (const FObjRawFace& Face : RawData.Faces)
	// {
	// 	GetOrAddMaterialSlot(Face.MaterialName);
	// }
	
	// IndexBuffer를 위한 Map
	TMap<FObjVertexKey, uint32> VertexMap;
	TArray<TArray<uint32>> SlotIndices;

	//	어차피 Section.size() == 0
	// SlotIndices.resize(StaticMeshAsset.Sections.size());

	for (const FObjRawFace& Face : RawData.Faces)
	{
		if (Face.Vertices.size() < 3)
		{
			continue;
		}

		const FString MaterialName = Face.MaterialName.empty() ? FString("Default") : Face.MaterialName;
		const int32 SlotIdx = GetOrAddMaterialSlot(MaterialName);
		
		if (SlotIdx >= static_cast<int32>(SlotIndices.size()))
		{
			SlotIndices.resize(SlotIdx + 1);
		}

		TArray<uint32>& IndicesPerSlot = SlotIndices[SlotIdx];
		
		for (uint32 i = 0; i < Face.Vertices.size() - 2; i++)
		{
			const uint32 I0 = GetOrCreateVertexIndex(Face.Vertices[0], VertexMap);
			const uint32 I1 = GetOrCreateVertexIndex(Face.Vertices[i + 1], VertexMap);
			const uint32 I2 = GetOrCreateVertexIndex(Face.Vertices[i + 2], VertexMap);

			IndicesPerSlot.push_back(I0);
			IndicesPerSlot.push_back(I1);
			IndicesPerSlot.push_back(I2);
		}
	}

	for (int32 SlotIdx = 0; SlotIdx < static_cast<int32>(SlotIndices.size()); SlotIdx++)
	{
		TArray<uint32>& IndicesPerSlot = SlotIndices[SlotIdx];
		if (IndicesPerSlot.empty())
		{
			continue;
		}

		FStaticMeshSection NewSection;
		NewSection.StartIndex = static_cast<int32>(StaticMeshAsset.Indices.size());
		NewSection.IndexCount = static_cast<uint32>(IndicesPerSlot.size());
		NewSection.MaterialSlotIndex = SlotIdx;
		// NewSection.SlotName = (SlotIdx >= 0 && SlotIdx < static_cast<int32>(BuiltMaterialSlotName.size()))
		// 	? BuiltMaterialSlotName[SlotIdx]
		// 	: FString("Default");;

		StaticMeshAsset.Indices.insert(
			StaticMeshAsset.Indices.end(),
			IndicesPerSlot.begin(),
			IndicesPerSlot.end());

		
		StaticMeshAsset.Sections.push_back(NewSection);
	}
	
	StaticMeshAsset.SlotNames = BuiltMaterialSlotName;
	StaticMeshAsset.LocalBounds = BuildLocalBounds();

	return !StaticMeshAsset.Vertices.empty() && !StaticMeshAsset.Indices.empty();
}

// bool FObjLoader::BindMaterials()
// {
// 	if (RawData.ReferencedMtlPath.empty())
// 		return true;
//
// 	std::filesystem::path MtlPath =
// 		std::filesystem::path(SourcePath).parent_path() / RawData.ReferencedMtlPath;
// 	MtlPath = MtlPath.generic_wstring();
//
// 	if (!FResourceManager::Get().LoadMaterial(MtlPath.string()))
// 		return true;
//
// 	for (FStaticMeshMaterialSlot& Slot : StaticMeshAsset.MaterialSlots)
// 	{
// 		Slot.MaterialData = FResourceManager::Get().FindMaterial(Slot.SlotName);
// 	}
//
// 	return true;
// }

void FObjLoader::Reset()
{
	SourcePath.clear();

	RawData = {};
	StaticMeshAsset = {};
	
	BuiltMaterialSlotName.clear();
}

int32 FObjLoader::GetOrAddMaterialSlot(const FString& MaterialName)
{
	FString SlotName = MaterialName.empty() ? FString("Default") : MaterialName;
	
	for (int32 i = 0; i < static_cast<int32>(BuiltMaterialSlotName.size()); i++)
	{
		if (BuiltMaterialSlotName[i] == SlotName)
		{
			return i;
		}
	}
	
	//	없다면 생성
	BuiltMaterialSlotName.push_back(SlotName);
	return static_cast<int32>(BuiltMaterialSlotName.size() - 1);
}

FAABB FObjLoader::BuildLocalBounds() const
{
	FAABB Bounds;
	Bounds.Reset();

	for (const FNormalVertex& Vertex : StaticMeshAsset.Vertices)
	{
		Bounds.Expand(Vertex.Position);
	}

	return Bounds;
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
	TexCoord.Y = 1.0f - std::stof(Tokens[2]); 

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
	TArray<FString> Parts;
	Parts.reserve(3);

	size_t Start = 0;
	while (true)
	{
		size_t SlashPos = Token.find('/', Start);
		if (SlashPos == FString::npos)
		{
			Parts.push_back(Token.substr(Start));
			break;
		}

		Parts.push_back(Token.substr(Start, SlashPos - Start));
		Start = SlashPos + 1;

		if (Start > Token.size())
		{
			Parts.emplace_back();
			break;
		}
	}

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

// int32 FObjLoader::GetOrAddMaterialSlot(const FString& MaterialName)
// {
// 	FString SlotName = MaterialName;
// 	if (SlotName.empty())
// 	{
// 		SlotName = "Default";
// 	}
//
// 	//	이미 존재하는 MaterialSlot인지 확인
// 	for (int32 i = 0; i < static_cast<int32>(StaticMeshAsset.MaterialSlots.size()); i++)
// 	{
// 		if (StaticMeshAsset.MaterialSlots[i].SlotName == SlotName)
// 		{
// 			return i;
// 		}
// 	}
//
// 	FStaticMeshMaterialSlot NewSlot = {};
// 	NewSlot.SlotName = SlotName;
//
// 	StaticMeshAsset.MaterialSlots.push_back(NewSlot);
// 	return static_cast<int32>(StaticMeshAsset.MaterialSlots.size() - 1);
// }

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
	Vertex.Color = FColor{ 1.f, 1.f, 1.f, 1.f };

	if (RawIndex.UVIndex >= 0 && RawIndex.UVIndex < static_cast<int32>(RawData.UVs.size()))
	{
		Vertex.UVs = RawData.UVs[RawIndex.UVIndex];
	}
	else
	{
		Vertex.UVs = FVector2{ 0.0f, 0.0f };
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

void FObjLoader::NormalizeRawPositionsToUnitCube()
{
	if (RawData.Positions.empty())
	{
		return;
	}

	FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (const FVector& Position : RawData.Positions)
	{
		Min.X = std::min(Min.X, Position.X);
		Min.Y = std::min(Min.Y, Position.Y);
		Min.Z = std::min(Min.Z, Position.Z);

		Max.X = std::max(Max.X, Position.X);
		Max.Y = std::max(Max.Y, Position.Y);
		Max.Z = std::max(Max.Z, Position.Z);
	}

	const FVector Center = (Min + Max) * 0.5f;
	const FVector Size = Max - Min;
	const float MaxDim = std::max(Size.X, std::max(Size.Y, Size.Z));

	if (MaxDim <= MathUtil::Epsilon)
	{
		return;
	}

	const float Scale = 1.0f / MaxDim;

	for (FVector& Position : RawData.Positions)
	{
		Position = (Position - Center) * Scale;
	}
}

void FObjLoader::NormalizeRawSizeToUnitCube()
{
	if (RawData.Positions.empty())
	{
		return;
	}

	FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (const FVector& Position : RawData.Positions)
	{
		Min.X = std::min(Min.X, Position.X);
		Min.Y = std::min(Min.Y, Position.Y);
		Min.Z = std::min(Min.Z, Position.Z);

		Max.X = std::max(Max.X, Position.X);
		Max.Y = std::max(Max.Y, Position.Y);
		Max.Z = std::max(Max.Z, Position.Z);
	}

	const FVector Center = (Min + Max) * 0.5f;
	
	for (FVector& Position : RawData.Positions)
	{
		Position = (Position - Center);
	}
}



#pragma endregion
