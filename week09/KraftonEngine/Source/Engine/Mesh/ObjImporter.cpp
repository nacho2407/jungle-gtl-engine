#include "Mesh/ObjImporter.h"
#include "Mesh/StaticMeshAsset.h"
#include "Materials/Material.h"
#include "Editor/UI/EditorConsolePanel.h"
#include "Engine/Platform/Paths.h"
#include "Mesh/ObjManager.h"
#include "SimpleJSON/json.hpp"
#include "Materials/MaterialManager.h"
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

static FString ExtractMtlTexturePath(std::string_view Line)
{
	std::string TextureFileName;

	while (!Line.empty())
	{
		std::string_view LineBeforeToken = Line;
		std::string_view Token = FStringParser::GetNextWhitespaceToken(Line);

		if (Token.empty()) break;

		if (Token[0] == '-')
		{
			int32 ArgsToSkip = 0;
			if (Token == "-s" || Token == "-o" || Token == "-t")
			{
				ArgsToSkip = 3;
			}
			else if (Token == "-mm")
			{
				ArgsToSkip = 2;
			}
			else if (Token == "-bm" || Token == "-boost" || Token == "-texres" ||
					 Token == "-blendu" || Token == "-blendv" || Token == "-clamp" ||
					 Token == "-cc" || Token == "-imfchan" || Token == "-type")
			{
				ArgsToSkip = 1;
			}

			for (int32 i = 0; i < ArgsToSkip; ++i)
			{
				FStringParser::GetNextWhitespaceToken(Line);
			}
		}
		else
		{
			FStringParser::TrimLeft(LineBeforeToken);
			TextureFileName = FString(LineBeforeToken);
			break;
		}
	}

	const size_t LastNonSpace = TextureFileName.find_last_not_of(" \t");
	if (LastNonSpace != FString::npos)
	{
		TextureFileName.erase(LastNonSpace + 1);
	}

	return TextureFileName;
}

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

	std::ifstream File(FPaths::ToWide(ObjFilePath), std::ios::binary | std::ios::ate);
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

	// UTF-8 BOM 스킵
	if (FileView.size() >= 3 && FileView[0] == '\xEF' && FileView[1] == '\xBB' && FileView[2] == '\xBF')
	{
		FileView.remove_prefix(3);
	}

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
	std::ifstream File(FPaths::ToWide(MtlFilePath), std::ios::binary | std::ios::ate);

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

	// UTF-8 BOM 스킵
	if (FileView.size() >= 3 && FileView[0] == '\xEF' && FileView[1] == '\xBB' && FileView[2] == '\xBF')
	{
		FileView.remove_prefix(3);
	}

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
			OutMtlInfos.emplace_back(MaterialInfo);
		}
		else if (Prefix == "Ka")
		{
			if (OutMtlInfos.empty())
			{
				continue;
			}
			FObjMaterialInfo& CurrentMaterial = OutMtlInfos.back();
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Ka.X);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Ka.Y);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Ka.Z);
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
		else if (Prefix == "Ks")
		{
			if (OutMtlInfos.empty())
			{
				continue;
			}
			FObjMaterialInfo& CurrentMaterial = OutMtlInfos.back();
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Ks.X);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Ks.Y);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Ks.Z);
		}
		else if (Prefix == "Ns")
		{
			if (!OutMtlInfos.empty())
			{
				FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), OutMtlInfos.back().Ns);
			}
		}
		else if (Prefix == "Ni")
		{
			if (!OutMtlInfos.empty())
			{
				FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), OutMtlInfos.back().Ni);
			}
		}
		else if (Prefix == "illum")
		{
			if (!OutMtlInfos.empty())
			{
				FStringParser::ParseInt(FStringParser::GetNextWhitespaceToken(Line), OutMtlInfos.back().illum);
			}
		}
		else if (Prefix == "map_Kd" || Prefix == "map_Ks" || Prefix == "map_Bump" || Prefix == "map_bump" || Prefix == "bump" || Prefix == "norm")
		{
			if (OutMtlInfos.empty())
			{
				continue;
			}

			const FString TextureFileName = ExtractMtlTexturePath(Line);
			if (!TextureFileName.empty())
			{
				const FString ResolvedPath = FPaths::ResolveAssetPath(MtlFilePath, TextureFileName);
				if (Prefix == "map_Kd")
				{
					OutMtlInfos.back().map_Kd = ResolvedPath;
				}
				else if (Prefix == "map_Ks")
				{
					OutMtlInfos.back().map_Ks = ResolvedPath;
				}
				else
				{
					OutMtlInfos.back().map_Bump = ResolvedPath;
				}
			}
		}
	}

	return true;
}

// MTL 정보에서 머티리얼 JSON 파일로 변환하는 함수
FString FObjImporter::ConvertMtlInfoToJson(const FObjMaterialInfo* MtlInfo)
{
	FString JsonPath = "Asset/Materials/" + MtlInfo->MaterialSlotName + ".json";

	// 이미 존재하면 덮어쓰지 않음 (에디터에서 수정했을 수 있으므로)
	if (std::filesystem::exists(FPaths::ToWide(JsonPath)))
		return JsonPath;

	json::JSON JsonData;
	JsonData["PathFileName"] = JsonPath;
	JsonData["ShaderPath"] = "Shaders/Materials/StaticMeshShader.hlsl";
	JsonData["RenderPass"] = "Opaque";
	JsonData["BlendState"] = "Opaque";
	JsonData["DepthStencilState"] = "Default";
	JsonData["RasterizerState"] = "SolidBackCull";

	if (!MtlInfo->map_Kd.empty())
	{
		JsonData["Textures"]["DiffuseTexture"] = MtlInfo->map_Kd;

		JsonData["Parameters"]["SectionColor"][0] = 1.0f;
		JsonData["Parameters"]["SectionColor"][1] = 1.0f;
		JsonData["Parameters"]["SectionColor"][2] = 1.0f;
		JsonData["Parameters"]["SectionColor"][3] = 1.0f;
	}
	else
	{
		JsonData["Parameters"]["SectionColor"][0] = MtlInfo->Kd.X;
		JsonData["Parameters"]["SectionColor"][1] = MtlInfo->Kd.Y;
		JsonData["Parameters"]["SectionColor"][2] = MtlInfo->Kd.Z;
		JsonData["Parameters"]["SectionColor"][3] = 1.0f;
	}

	if (!MtlInfo->map_Bump.empty())
	{
		JsonData["Textures"]["NormalTexture"] = MtlInfo->map_Bump;
	}

	if (!MtlInfo->map_Ks.empty())
	{
		JsonData["Textures"]["SpecularTexture"] = MtlInfo->map_Ks;
	}

	JsonData["Parameters"]["SpecularPower"] = (MtlInfo->Ns > 0.0f) ? MtlInfo->Ns : 32.0f;
	const float AvgSpecular = (MtlInfo->Ks.X + MtlInfo->Ks.Y + MtlInfo->Ks.Z) / 3.0f;
	JsonData["Parameters"]["SpecularStrength"] = (AvgSpecular > 0.0f) ? AvgSpecular : 1.0f;

	std::ofstream File(FPaths::ToWide(JsonPath));
	File << JsonData.dump();

	return JsonPath;
}

FVector FObjImporter::RemapPosition(const FVector& ObjPos, EForwardAxis Axis)
{
	// OBJ 원본 좌표 (Ox, Oy, Oz) → 엔진 (Ex, Ey, Ez)
	// 엔진: X=Forward, Y=Right, Z=Up
	// OBJ 기본: Y-up 우수 좌표계
	switch (Axis)
	{
	case EForwardAxis::X:    // OBJ +X → Engine Forward(+X)
		return FVector(ObjPos.X, ObjPos.Z, ObjPos.Y);
	case EForwardAxis::NegX: // OBJ -X → Engine Forward(+X)
		return FVector(-ObjPos.X, -ObjPos.Z, ObjPos.Y);
	case EForwardAxis::Y:    // OBJ +Y → Engine Forward(+X)
		return FVector(ObjPos.Y, ObjPos.X, ObjPos.Z);
	case EForwardAxis::NegY: // OBJ -Y → Engine Forward(+X) — 블렌더 기본
		return FVector(-ObjPos.Y, -ObjPos.X, ObjPos.Z);
	case EForwardAxis::Z:    // OBJ +Z → Engine Forward(+X)
		return FVector(ObjPos.Z, ObjPos.X, ObjPos.Y);
	case EForwardAxis::NegZ: // OBJ -Z → Engine Forward(+X) — OBJ 기본 (Y-up, -Z forward)
		return FVector(-ObjPos.Z, ObjPos.X, ObjPos.Y);
	default:
		return FVector(ObjPos.X, ObjPos.Z, ObjPos.Y);
	}
}

bool FObjImporter::Convert(const FObjInfo& ObjInfo, const TArray<FObjMaterialInfo>& MtlInfos, const FImportOptions& Options, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
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
			UE_LOG("Importer TargetSlotName: %s;", TargetSlotName.c_str());

			// Convert() 안에서 기존 직접 세팅 대신
			FString JsonPath = ConvertMtlInfoToJson(MatchedMaterial); // .json 파일 생성
			UMaterial* MaterialObject = FMaterialManager::Get().GetOrCreateStaticMeshMaterial(JsonPath);

			// FStaticMaterial 슬롯 생성 및 OutMaterials에 추가
			FStaticMaterial NewStaticMaterial;
			NewStaticMaterial.MaterialInterface = MaterialObject;
			NewStaticMaterial.MaterialSlotName = TargetSlotName;
			OutMaterials.push_back(NewStaticMaterial);
		}
		else // Material Slot이 MTL 파일에 정의되어 있지 않은 경우
		{
			UMaterial* DefaultMaterial = FMaterialManager::Get().GetOrCreateMaterial("None");

			// FStaticMaterial 슬롯 생성 및 OutMaterials에 추가
			FStaticMaterial NewEmptyStaticMaterial;
			NewEmptyStaticMaterial.MaterialInterface = DefaultMaterial;
			NewEmptyStaticMaterial.MaterialSlotName = TargetSlotName;
			OutMaterials.push_back(NewEmptyStaticMaterial);
		}
	}

	// "None" 슬롯이 존재했다면 맨 마지막에 배치
	if (bHasNoneSlot)
	{
		UMaterial* DefaultMaterial = FMaterialManager::Get().GetOrCreateMaterial("None");

		FStaticMaterial NewDefaultStaticMaterial;
		NewDefaultStaticMaterial.MaterialInterface = DefaultMaterial;
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
		NewSection.MaterialIndex = static_cast<int32>(MaterialIndex);
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

			// --- Tangent / Bitangent 계산 ---
			FVector T(1, 0, 0), B(0, 1, 0);
			if (ObjInfo.UVIndices[FaceStartIndex] != -1)
			{
				FVector2 uv0 = ObjInfo.UVs[ObjInfo.UVIndices[FaceStartIndex]];
				FVector2 uv1 = ObjInfo.UVs[ObjInfo.UVIndices[FaceStartIndex + 1]];
				FVector2 uv2 = ObjInfo.UVs[ObjInfo.UVIndices[FaceStartIndex + 2]];

				FVector2 deltaUV1 = uv1 - uv0;
				FVector2 deltaUV2 = uv2 - uv0;

				float f = 1.0f / (deltaUV1.X * deltaUV2.Y - deltaUV2.X * deltaUV1.Y);
				if (!std::isinf(f) && !std::isnan(f))
				{
					T.X = f * (deltaUV2.Y * Edge1.X - deltaUV1.Y * Edge2.X);
					T.Y = f * (deltaUV2.Y * Edge1.Y - deltaUV1.Y * Edge2.Y);
					T.Z = f * (deltaUV2.Y * Edge1.Z - deltaUV1.Y * Edge2.Z);

					B.X = f * (-deltaUV2.X * Edge1.X + deltaUV1.X * Edge2.X);
					B.Y = f * (-deltaUV2.X * Edge1.Y + deltaUV1.X * Edge2.Y);
					B.Z = f * (-deltaUV2.X * Edge1.Z + deltaUV1.X * Edge2.Z);
				}
			}
			T.Normalize();
			B.Normalize();

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
					FVertexPNCT_T NewVertex;

					// 축 리맵 + 스케일 적용
					NewVertex.Position = RemapPosition(ObjInfo.Positions[Key.p], Options.ForwardAxis) * Options.Scale;

					// Normal 리맵
					FVector FinalNormal;
					if (Key.n == -1)
					{
						FinalNormal = RemapPosition(FaceNormal, Options.ForwardAxis).Normalized();
					}
					else
					{
						FinalNormal = RemapPosition(ObjInfo.Normals[Key.n], Options.ForwardAxis).Normalized();
					}
					NewVertex.Normal = FinalNormal;

					// UV 예외 처리
					if (Key.t == -1)
					{
						NewVertex.UV = { 0.0f, 0.0f };
					}
					else
					{
						NewVertex.UV = ObjInfo.UVs[Key.t];
						// UV 변환 (left-bottom -> left-top)
						NewVertex.UV.V = 1.0f - NewVertex.UV.V;
					}

					NewVertex.Color = { 1.0f, 1.0f, 1.0f, 1.0f };

					// Tangent 리맵 및 Handedness(w) 계산
					FVector WorldT = RemapPosition(T, Options.ForwardAxis).Normalized();
					FVector WorldB = RemapPosition(B, Options.ForwardAxis).Normalized();
					float w = (FinalNormal.Cross(WorldT).Dot(WorldB) < 0.0f) ? -1.0f : 1.0f;
					NewVertex.Tangent = FVector4(WorldT.X, WorldT.Y, WorldT.Z, w);

					uint32 NewIndex = static_cast<uint32>(OutMesh.Vertices.size());
					OutMesh.Vertices.push_back(NewVertex);

					VertexMap[Key] = NewIndex;
					TriangleIndices[j] = NewIndex;
				}
			}

			// 와인딩 오더 처리
			OutMesh.Indices.push_back(TriangleIndices[0]);
			if (Options.WindingOrder == EWindingOrder::CCW_to_CW)
			{
				OutMesh.Indices.push_back(TriangleIndices[2]);
				OutMesh.Indices.push_back(TriangleIndices[1]);
			}
			else
			{
				OutMesh.Indices.push_back(TriangleIndices[1]);
				OutMesh.Indices.push_back(TriangleIndices[2]);
			}
		}

		OutMesh.Sections.push_back(NewSection);
	}

    return true;
}

bool FObjImporter::Import(const FString& ObjFilePath, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
	return Import(ObjFilePath, FImportOptions::Default(), OutMesh, OutMaterials);
}

bool FObjImporter::Import(const FString& ObjFilePath, const FImportOptions& Options, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
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
			ObjInfo.MaterialLibraryFilePath.clear();
			ParsedMtlInfos.clear();
		}
	}

	if (!FObjImporter::Convert(ObjInfo, ParsedMtlInfos, Options, OutMesh, OutMaterials)){
		UE_LOG("Convert failed for: %s", ObjFilePath.c_str());
		return false;
	}
	OutMesh.PathFileName = ObjFilePath;

	auto EndTime = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> Duration = EndTime - StartTime;
	UE_LOG("OBJ Imported successfully. File: %s. Time taken: %.4f seconds", ObjFilePath.c_str(), Duration.count());

	return true;
}
