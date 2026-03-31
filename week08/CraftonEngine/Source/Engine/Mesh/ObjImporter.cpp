#include "Mesh/ObjImporter.h"
#include "Mesh/StaticMeshAsset.h"
#include "Materials/Material.h"
#include "Editor/UI/EditorConsoleWidget.h"
#include "Engine/Platform/Paths.h"
#include "Mesh/ObjManager.h"
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <charconv>
#include <chrono>

const FVector FallbackColor3 = FVector(1.0f, 0.0f, 1.0f);
const FVector4 FallbackColor4 = FVector4(1.0f, 0.0f, 1.0f, 1.0f);

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

struct FStringParser
{
	// Delimiter로 구분된 다음 토큰을 추출하고, InOutView에서 해당 토큰과 구분자 제거
	static std::string_view GetNextToken(std::string_view& InOutView, char Delimiter = ' ')
	{
		size_t DelimiterPosition = InOutView.find(Delimiter);
		std::string_view Token = InOutView.substr(0, DelimiterPosition); // [0, DelimiterPosition) 범위의 토큰 추출
		if (DelimiterPosition != std::string_view::npos)
		{
			InOutView.remove_prefix(DelimiterPosition + 1); // 토큰과 구분자 제거
		}
		else
		{
			InOutView = std::string_view();
		}
		return Token;
	}

	// 다수의 공백을 구분자로 사용하여 다음 토큰을 추출하고, InOutView에서 해당 토큰과 앞의 공백 제거
	static std::string_view GetNextWhitespaceToken(std::string_view& InOutView)
	{
		size_t Start = InOutView.find_first_not_of(" \t");
		if (Start == std::string_view::npos)
		{
			InOutView = std::string_view();
			return std::string_view();
		}
		InOutView.remove_prefix(Start); // 유효한 문자 앞의 공백 제거

		size_t End = InOutView.find_first_of(" \t");
		std::string_view Token = InOutView.substr(0, End); // 공백 이전까지의 토큰 추출

		if (End != std::string_view::npos)
		{
			InOutView.remove_prefix(End);
		}
		else
		{
			InOutView = std::string_view();
		}
		return Token;
	}

	// InOutView의 왼쪽 끝에 있는 공백 제거
	static void TrimLeft(std::string_view& InOutView)
	{
		size_t Start = InOutView.find_first_not_of(" \t");
		if (Start != std::string_view::npos)
		{
			InOutView.remove_prefix(Start);  // 유효한 문자 앞의 공백 제거
		}
		else
		{
			InOutView = std::string_view();
		}
	}

	static bool ParseInt(std::string_view Str, int& OutValue)
	{
		if (Str.empty()) return false;
		std::from_chars_result result = std::from_chars(Str.data(), Str.data() + Str.size(), OutValue);
		return result.ec == std::errc();
	}

	static bool ParseFloat(std::string_view Str, float& OutValue)
	{
		if (Str.empty()) return false;
		std::from_chars_result result = std::from_chars(Str.data(), Str.data() + Str.size(), OutValue);
		return result.ec == std::errc();
	}
};

struct FRawFaceVertex
{
    int32 PosIndex = -1;
    int32 UVIndex = -1;
    int32 NormalIndex = -1;
};

FRawFaceVertex ParseSingleFaceVertex(std::string_view FaceToken)
{
    FRawFaceVertex Result;

    // 첫 번째 토큰: Position
    std::string_view PosStr = FStringParser::GetNextToken(FaceToken, '/');
    FStringParser::ParseInt(PosStr, Result.PosIndex);

    // 두 번째 토큰: UV (있을 수도, 비어있을 수도 있음)
    if (!FaceToken.empty())
    {
        std::string_view UVStr = FStringParser::GetNextToken(FaceToken, '/');
        if (!UVStr.empty())
        {
            FStringParser::ParseInt(UVStr, Result.UVIndex);
        }
    }

    // 세 번째 토큰: Normal
    if (!FaceToken.empty())
    {
        std::string_view NormalStr = FStringParser::GetNextToken(FaceToken, '/');
        FStringParser::ParseInt(NormalStr, Result.NormalIndex);
    }

    return Result;
}

bool FObjImporter::ParseObj(const FString& ObjFilePath, FObjInfo& OutObjInfo)
{
	OutObjInfo = FObjInfo();

	std::ifstream File(ObjFilePath, std::ios::binary | std::ios::ate);
	if (!File.is_open())
	{
		UE_LOG("Failed to open OBJ file: %s", ObjFilePath.c_str());
		return false;
	}

	size_t FileSize = static_cast<size_t>(File.tellg());
	File.seekg(0, std::ios::beg);
	TArray<char> Buffer(FileSize);
	if (!File.read(Buffer.data(), FileSize))
	{
		UE_LOG("Failed to read OBJ file: %s", ObjFilePath.c_str());
		return false;
	}

	std::string_view FileView(Buffer.data(), Buffer.size());

	TArray<FRawFaceVertex> FaceVertices;
	FaceVertices.reserve(6); // Heuristic

	while (!FileView.empty())
	{
		std::string_view Line = FStringParser::GetNextToken(FileView, '\n');

		// CRLF 제거
		if (!Line.empty() && Line.back() == '\r')
		{
			Line.remove_suffix(1);
		}

		if (Line.empty() || Line[0] == '#')
		{
			continue;
		}

		std::string_view Prefix = FStringParser::GetNextToken(Line);

		if (Prefix == "v")
		{
			FVector Position;
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Position.X);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Position.Y);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Position.Z);
			OutObjInfo.Positions.emplace_back(Position);
		}
		else if (Prefix == "vt")
		{
			FVector2 UV;
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), UV.U);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), UV.V);
			OutObjInfo.UVs.emplace_back(UV);
		}
		else if (Prefix == "vn")
		{
			FVector Normal;
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Normal.X);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Normal.Y);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Normal.Z);
			OutObjInfo.Normals.emplace_back(Normal);
		}
		else if (Prefix == "f")
		{
			// default material section 추가 (usemtl이 없이 f가 먼저 나오는 경우)
			if (OutObjInfo.Sections.empty())
			{
				FStaticMeshSection DefaultSection;
				DefaultSection.MaterialSlotName = "None";
				DefaultSection.FirstIndex = 0;
				DefaultSection.NumTriangles = 0;
				OutObjInfo.Sections.emplace_back(DefaultSection);
			}

			while (!Line.empty())
			{
				std::string_view FaceToken = FStringParser::GetNextToken(Line, ' ');
				if (!FaceToken.empty())
				{
					FaceVertices.push_back(ParseSingleFaceVertex(FaceToken));
				}
			}

			if (FaceVertices.size() < 3)
			{
				UE_LOG("Face with less than 3 vertices");
				continue;
			}

			// Fan triangulation
			for (size_t i = 1; i + 1 < FaceVertices.size(); ++i)
			{
				const std::array<FRawFaceVertex, 3> TriangleVerts = { FaceVertices[0], FaceVertices[i], FaceVertices[i + 1] };
				for (int j = 0; j < 3; ++j)
				{
					constexpr int32 InvalidIndex = -1;
					OutObjInfo.PosIndices.emplace_back(TriangleVerts[j].PosIndex - 1);
					OutObjInfo.UVIndices.emplace_back(TriangleVerts[j].UVIndex > 0 ? TriangleVerts[j].UVIndex - 1 : InvalidIndex);
					OutObjInfo.NormalIndices.emplace_back(TriangleVerts[j].NormalIndex > 0 ? TriangleVerts[j].NormalIndex - 1 : InvalidIndex);
				}
			}
			FaceVertices.clear();
		}
		else
		{
			if (Prefix == "mtllib")
			{
				size_t CommentPos = Line.find('#');
				if (CommentPos != std::string_view::npos) { Line = Line.substr(0, CommentPos); }
				FStringParser::TrimLeft(Line);
				OutObjInfo.MaterialLibraryFilePath = FPaths::ResolveAssetPath(ObjFilePath, std::string(Line));
				UE_LOG("Found material library: %s", OutObjInfo.MaterialLibraryFilePath.c_str());
			}
			else if (Prefix == "usemtl")
			{
				size_t CommentPos = Line.find('#');
				if (CommentPos != std::string_view::npos) { Line = Line.substr(0, CommentPos); }
				FStringParser::TrimLeft(Line);

				if (!OutObjInfo.Sections.empty())
				{
					OutObjInfo.Sections.back().NumTriangles = (static_cast<uint32>(OutObjInfo.PosIndices.size()) - OutObjInfo.Sections.back().FirstIndex) / 3;
				}
				FStaticMeshSection Section;
				Section.MaterialSlotName = std::string(Line);
				if (Section.MaterialSlotName.empty())
				{
					Section.MaterialSlotName = "None";
				}
				Section.FirstIndex = static_cast<uint32>(OutObjInfo.PosIndices.size());
				OutObjInfo.Sections.emplace_back(Section);
			}
			else if (Prefix == "o")
			{
				size_t CommentPos = Line.find('#');
				if (CommentPos != std::string_view::npos) { Line = Line.substr(0, CommentPos); }
				FStringParser::TrimLeft(Line);

				OutObjInfo.ObjectName = std::string(Line);
			}
		}
	}

	if (!OutObjInfo.Sections.empty())
	{
		OutObjInfo.Sections.back().NumTriangles = (static_cast<uint32>(OutObjInfo.PosIndices.size()) - OutObjInfo.Sections.back().FirstIndex) / 3;
	}

	if (OutObjInfo.UVs.empty())
	{
		OutObjInfo.UVs.emplace_back(FVector2{ 0.0f, 0.0f });
	}

	return true;
}

bool FObjImporter::ParseMtl(const FString& MtlFilePath, TArray<FObjMaterialInfo>& OutMtlInfos)
{
	OutMtlInfos.clear();
	std::ifstream File(MtlFilePath, std::ios::binary | std::ios::ate);

	if (!File.is_open())
	{
		UE_LOG("Failed to open MTL file: %s", MtlFilePath.c_str());
		return false;
	}

	size_t FileSize = static_cast<size_t>(File.tellg());
	File.seekg(0, std::ios::beg);
	TArray<char> Buffer(FileSize);
	if (!File.read(Buffer.data(), FileSize))
	{
		UE_LOG("Failed to read MTL file: %s", MtlFilePath.c_str());
		return false;
	}

	std::string_view FileView(Buffer.data(), Buffer.size());

	while (!FileView.empty())
	{
		std::string_view Line = FStringParser::GetNextToken(FileView, '\n');

		// CRLF 제거
		if (!Line.empty() && Line.back() == '\r')
		{
			Line.remove_suffix(1);
		}

		if (Line.empty() || Line[0] == '#')
		{
			continue;
		}

		std::string_view Prefix = FStringParser::GetNextWhitespaceToken(Line);

		if (Prefix == "newmtl")
		{
			FObjMaterialInfo MaterialInfo;
			FStringParser::TrimLeft(Line);
			MaterialInfo.MaterialSlotName = std::string(Line);
			MaterialInfo.Kd = FallbackColor3;
			OutMtlInfos.emplace_back(MaterialInfo);
		}
		else if (Prefix == "Kd")
		{
			if (OutMtlInfos.empty())
			{
				continue;
			}
			FObjMaterialInfo& CurrentMaterial = OutMtlInfos.back();
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Kd.X);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Kd.Y);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Kd.Z);
		}
		else if (Prefix == "map_Kd")
		{
			if (OutMtlInfos.empty())
			{
				continue;
			}

			std::string TextureFileName;

			// 토큰 단위로 옵션들을 건너뜁니다.
			while (!Line.empty())
			{
				// 파일명에 공백이 포함될 수 있으므로, 현재 Line의 상태를 백업해둡니다.
				std::string_view LineBeforeToken = Line;
				std::string_view Token = FStringParser::GetNextWhitespaceToken(Line);

				if (Token.empty()) break;

				// 토큰이 '-'로 시작하면 옵션 플래그인지 확인합니다.
				if (Token[0] == '-')
				{
					int32 ArgsToSkip = 0;

					// 1. 3개의 인자를 받는 옵션 (Vector)
					if (Token == "-s" || Token == "-o" || Token == "-t")
					{
						ArgsToSkip = 3;
					}
					// 2. 2개의 인자를 받는 옵션
					else if (Token == "-mm")
					{
						ArgsToSkip = 2;
					}
					// 3. 1개의 인자를 받는 옵션 (Float, String, Bool)
					else if (Token == "-bm" || Token == "-boost" || Token == "-texres" ||
							 Token == "-blendu" || Token == "-blendv" || Token == "-clamp" ||
							 Token == "-cc" || Token == "-imfchan")
					{
						ArgsToSkip = 1;
					}

					// 파악된 옵션의 인자 개수만큼 다음 토큰들을 무시합니다.
					for (int32 i = 0; i < ArgsToSkip; ++i)
					{
						FStringParser::GetNextWhitespaceToken(Line);
					}
				}
				else
				{
					// '-'로 시작하지 않는 첫 번째 토큰을 만났다면, 이것이 파일명의 시작입니다!
					// 파일명 내부에 띄어쓰기가 있을 수 있으므로 토큰을 뽑기 전의 전체 라인을 가져옵니다.
					FStringParser::TrimLeft(LineBeforeToken);
					TextureFileName = FString(LineBeforeToken);
					break;
				}
			}

			// 문자열 끝에 남아있을지 모르는 쓸데없는 공백이나 탭을 정리합니다. (RTrim)
			size_t LastNonSpace = TextureFileName.find_last_not_of(" \t");
			if (LastNonSpace != FString::npos)
			{
				TextureFileName.erase(LastNonSpace + 1);
			}

			// 최종적으로 추출된 파일명 할당
			if (!TextureFileName.empty())
			{
				OutMtlInfos.back().map_Kd = FPaths::ResolveAssetPath(MtlFilePath, TextureFileName);
			}
		}
	}

	return true;
}

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

		// 기존에 수집된 슬롯과 중복되지 않는 경우에만 추가
		if (std::find(OrderedMaterialSlots.begin(), OrderedMaterialSlots.end(), CurrentSlotName) == OrderedMaterialSlots.end())
		{
			OrderedMaterialSlots.push_back(CurrentSlotName);
		}
	}

	UMaterial* DefaultMaterialObject = FObjManager::GetOrLoadMaterial("None");
	if (DefaultMaterialObject->PathFileName.empty())
	{
		DefaultMaterialObject->PathFileName = "None";
		DefaultMaterialObject->DiffuseColor = FallbackColor4;
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
			// 섹션 머티리얼 슬롯 이름과 일치하는 머티리얼 이름이 MTL 파일에서 발견된 경우, 해당 머티리얼 로드 또는 생성
			UMaterial* MaterialObject = FObjManager::GetOrLoadMaterial(TargetSlotName);

			// 머티리얼 객체가 새로 생성된 경우에만 속성 설정 (캐시에서 로드된 경우 이미 설정되어 있다고 가정)
			if (MaterialObject->PathFileName.empty())
			{
				MaterialObject->PathFileName = TargetSlotName;

				if (!MatchedMaterial->map_Kd.empty())
				{
					MaterialObject->DiffuseTextureFilePath = MatchedMaterial->map_Kd;
					MaterialObject->DiffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
				}
				else
				{
					MaterialObject->DiffuseColor = { MatchedMaterial->Kd.X, MatchedMaterial->Kd.Y, MatchedMaterial->Kd.Z, 1.0f };
				}
			}

			// FStaticMaterial 슬롯 생성 및 OutMaterials에 추가
			FStaticMaterial NewStaticMaterial;
			NewStaticMaterial.MaterialInterface = MaterialObject;
			NewStaticMaterial.MaterialSlotName = TargetSlotName;
			OutMaterials.push_back(NewStaticMaterial);
		}
		else // Material Slot이 MTL 파일에 정의되어 있지 않은 경우
		{
			// FStaticMaterial 슬롯 생성 및 OutMaterials에 추가
			FStaticMaterial NewEmptyStaticMaterial;
			NewEmptyStaticMaterial.MaterialInterface = DefaultMaterialObject;
			NewEmptyStaticMaterial.MaterialSlotName = TargetSlotName;
			OutMaterials.push_back(NewEmptyStaticMaterial);
		}
	}

	// "None" 슬롯이 존재했다면 맨 마지막에 배치
	if (bHasNoneSlot)
	{
		FStaticMaterial NewDefaultStaticMaterial;
		NewDefaultStaticMaterial.MaterialInterface = DefaultMaterialObject;
		NewDefaultStaticMaterial.MaterialSlotName = "None";

		OutMaterials.push_back(NewDefaultStaticMaterial);
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

			// (P1 - P0) X (P2 - P0) 후 정규화
			FVector P0 = ObjInfo.Positions[ObjInfo.PosIndices[FaceStartIndex]];
			FVector P1 = ObjInfo.Positions[ObjInfo.PosIndices[FaceStartIndex + 1]];
			FVector P2 = ObjInfo.Positions[ObjInfo.PosIndices[FaceStartIndex + 2]];

			FVector Edge1 = P1 - P0;
			FVector Edge2 = P2 - P0;
			FVector FaceNormal = Edge1.Cross(Edge2).Normalized();

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

					// Normal 예외 처리
					if (Key.n == -1)
					{
						NewVertex.normal = FaceNormal;
					}
					else
					{
						NewVertex.normal = ObjInfo.Normals[Key.n];
					}

					// OBJ는 Y-up, Z-Forward (RHS)
					// UE는 Z-Up, X-Forward (LHS)

					float RawY = ObjInfo.Positions[Key.p].Y;
					float RawZ = ObjInfo.Positions[Key.p].Z;

					NewVertex.pos.X = ObjInfo.Positions[Key.p].X;
					NewVertex.pos.Y = RawZ;
					NewVertex.pos.Z = RawY;

					// UV 예외 처리
					if (Key.t == -1)
					{
						NewVertex.tex = { 0.0f, 0.0f };
					}
					else
					{
						NewVertex.tex = ObjInfo.UVs[Key.t];
						// UV 변환 (left-bottom -> left-top)
						NewVertex.tex.V = 1.0f - NewVertex.tex.V;
					}

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
	auto StartTime = std::chrono::high_resolution_clock::now();

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

	auto EndTime = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> Duration = EndTime - StartTime;
	UE_LOG("OBJ Imported successfully. File: %s. Time taken: %.4f seconds", ObjFilePath.c_str(), Duration.count());

	return true;
}
