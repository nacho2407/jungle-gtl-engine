#include "ObjManager.h"
#include "Core/Paths.h"
#include <fstream>
#include <sstream>

#include "Core/Engine.h"
#include "Debug/EngineLog.h"
#include "Renderer/Renderer.h"
#include "Math/MathUtility.h"
#include "Renderer/MaterialManager.h"
#include "Renderer/Shader.h"
#include "Renderer/Material.h"
#include "Renderer/ShaderMap.h"

TMap<FString, UStaticMesh*> FObjManager::ObjStaticMeshMap;

// Obj 파싱 전용 컨텍스트 - 이 cpp 파일 안에서만 사용됨
// 임시 데이터와 파싱 상태를 한 곳에서 관리.
struct FObjParserContext
{
	FStaticMesh* OutMesh;
	TArray<FString>& OutMaterialNames;

	TArray<FVector> TempPositions;
	TArray<FVector2> TempUVs;
	TArray<FVector> TempNormals;

	std::string Line;
	uint32 CurrentSectionStartIndex = 0;
	int32 CurrentMaterialIndex = -1;

	void CloseCurrentSection()
	{
		if (OutMesh->Indices.size() > CurrentSectionStartIndex)
		{
			FMeshSection Section;
			Section.MaterialIndex = CurrentMaterialIndex;
			Section.StartIndex = CurrentSectionStartIndex;
			Section.IndexCount = static_cast<uint32>(OutMesh->Indices.size()) - CurrentSectionStartIndex;
			OutMesh->Sections.push_back(Section);

			CurrentSectionStartIndex = static_cast<uint32>(OutMesh->Indices.size());
		}
	}

	void ParseUseMtl(std::stringstream& SS)
	{
		std::string MaterialName;
		SS >> MaterialName;

		CloseCurrentSection();

		FString NewMaterialName(MaterialName.c_str());
		CurrentMaterialIndex = OutMaterialNames.size();
		OutMaterialNames.push_back(NewMaterialName);
	}

	void ParseFace(std::stringstream& SS)
	{
		if (CurrentMaterialIndex == -1)
		{
			CurrentMaterialIndex = 0;
			OutMaterialNames.push_back("M_Default");
		}

		std::string VStr;
		struct FIndex { uint32 PositionIndex; uint32 UVIndex; uint32 NormalIndex; };
		TArray<FIndex> Face;

		while (SS >> VStr)
		{
			std::stringstream VSS(VStr);
			std::string PositionString, UVString, NormalString;

			std::getline(VSS, PositionString, '/');
			std::getline(VSS, UVString, '/');
			std::getline(VSS, NormalString, '/');

			FIndex Idx{};
			Idx.PositionIndex = std::stoi(PositionString) - 1;
			Idx.UVIndex = UVString.empty() ? 0 : std::stoi(UVString) - 1;
			Idx.NormalIndex = NormalString.empty() ? 0 : std::stoi(NormalString) - 1;

			Face.push_back(Idx);
		}

		// 다각형을 삼각형으로 쪼개기
		for (size_t i = 1; i + 1 < Face.size(); ++i)
		{
			uint32 BaseIndex = static_cast<uint32>(OutMesh->Vertices.size());

			auto AddVertex = [&](const FIndex& Idx)
				{
					FVertex V{};
					V.Position = TempPositions[Idx.PositionIndex];
					V.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

					if (!TempUVs.empty() && Idx.UVIndex < TempUVs.size()) V.UV = TempUVs[Idx.UVIndex];
					if (!TempNormals.empty() && Idx.NormalIndex < TempNormals.size()) V.Normal = TempNormals[Idx.NormalIndex];

					OutMesh->Vertices.push_back(V);
				};

			AddVertex(Face[0]);
			AddVertex(Face[i]);
			AddVertex(Face[i + 1]);

			OutMesh->Indices.push_back(BaseIndex + 0);
			OutMesh->Indices.push_back(BaseIndex + 1);
			OutMesh->Indices.push_back(BaseIndex + 2);
		}
	}
};

inline UStaticMesh* FObjManager::LoadObjStaticMeshAsset(const FString& PathFileName)
{
	// 추후에 obj파싱이 끝나면 없앨 코드
	if (PathFileName == "Primitive_Sphere") return GetPrimitiveSphere();

	// ---------------------------------------------
	auto It = ObjStaticMeshMap.find(PathFileName);
	if (It != ObjStaticMeshMap.end()) return It->second;

	FStaticMesh* RawData = new FStaticMesh();
	RawData->PathFileName = PathFileName;

	TArray<FString> FoundMaterials;
	if (!ParseObjFile(PathFileName, RawData, FoundMaterials))
	{
		delete RawData;
		return nullptr;
	}

	RawData->UpdateLocalBound();

	UStaticMesh* NewAsset = new UStaticMesh();
	NewAsset->SetStaticMeshAsset(RawData);
	
	NewAsset->LocalBounds.Radius = RawData->GetLocalBoundRadius();
	NewAsset->LocalBounds.Center = RawData->GetCenterCoord();
	NewAsset->LocalBounds.BoxExtent = (RawData->GetMaxCoord() - RawData->GetMinCoord()) * 0.5f;

	for (const FString& MatName : FoundMaterials)
	{
		UE_LOG("%s", MatName.c_str());
		auto Material = FMaterialManager::Get().FindByName(MatName);

		if (!Material)
		{
			UE_LOG("[경고] OBJ에서 요청한 머티리얼 '%s'가 없습니다. 기본 머티리얼로 대체합니다.", MatName.c_str());
			Material = FMaterialManager::Get().FindByName("M_Default");
		}

		NewAsset->AddDefaultMaterial(Material);
	}

	if (FoundMaterials.empty())
	{
		NewAsset->AddDefaultMaterial(FMaterialManager::Get().FindByName("M_Default"));
	}
	ObjStaticMeshMap[PathFileName] = NewAsset;

	return NewAsset;
}

//TODO: Texture Manager 리펙토링 이후 이 함수는 삭제될 것.
bool FObjManager::ParseMtlFile(const FString& MtlFIlePath)
{
	std::ifstream File(MtlFIlePath.c_str());
	if (!File.is_open()) return false;

	std::string Line;
	std::shared_ptr<FMaterial> CurrentMaterial = nullptr;

	while (std::getline(File, Line))
	{
		if (Line.empty() || Line[0] == '#') continue;

		std::stringstream SS(Line);
		std::string Type;
		SS >> Type;

		if (Type == "newmtl")
		{
			std::string MaterialName;
			SS >> MaterialName;

			CurrentMaterial = std::make_shared<FMaterial>();
			CurrentMaterial->SetOriginName(MaterialName.c_str());

			std::wstring VSPath = FPaths::ShaderDir() / L"VertexShader.hlsl";
			std::wstring PSPath = FPaths::ShaderDir() / L"TexturePixelShader.hlsl";
			CurrentMaterial->SetVertexShader(FShaderMap::Get().GetOrCreateVertexShader(GEngine->GetRenderer()->GetDevice(), VSPath.c_str()));
			CurrentMaterial->SetPixelShader(FShaderMap::Get().GetOrCreatePixelShader(GEngine->GetRenderer()->GetDevice(), PSPath.c_str()));

			auto DefaultTexMat = GEngine->GetRenderer()->GetDefaultTextureMaterial();
			CurrentMaterial->SetRasterizerOption(DefaultTexMat->GetRasterizerOption());
			CurrentMaterial->SetRasterizerState(DefaultTexMat->GetRasterizerState());
			CurrentMaterial->SetDepthStencilOption(DefaultTexMat->GetDepthStencilOption());
			CurrentMaterial->SetDepthStencilState(DefaultTexMat->GetDepthStencilState());

			int32 SlotIndex = CurrentMaterial->CreateConstantBuffer(GEngine->GetRenderer()->GetDevice(), 16);
			if (SlotIndex >= 0)
			{
				CurrentMaterial->RegisterParameter("BaseColor", SlotIndex, 0, 16);
				float White[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
				CurrentMaterial->GetConstantBuffer(SlotIndex)->SetData(White, sizeof(White));
			}

			FMaterialManager::Get().Register(MaterialName.c_str(), CurrentMaterial);
		}
		else if (Type == "map_Kd" && CurrentMaterial)
		{
			std::string TextureFileName;
			SS >> TextureFileName;

			std::filesystem::path TexturePath = FPaths::MeshDir() / TextureFileName;

			ID3D11ShaderResourceView* NewSRV = nullptr;
			if (GEngine->GetRenderer()->CreateTextureFromSTB(GEngine->GetRenderer()->GetDevice(), TexturePath.string().c_str(), &NewSRV))
			{
				auto MaterialTexture = std::make_shared<FMaterialTexture>();
				MaterialTexture->TextureSRV = NewSRV;

				CurrentMaterial->SetMaterialTexture(MaterialTexture);
				UE_LOG("[MTL 파서] %s 텍스처 자동 로드 및 장착 완료!", TextureFileName.c_str());
			}
		}
	}
	return true;
}

inline bool FObjManager::ParseObjFile(const FString& FilePath, FStaticMesh* OutMesh, TArray<FString>& OutMaterialNames)
{
	std::string FilePathStr(FilePath.c_str());
	std::ifstream File(FilePathStr); // 텍스트 모드로 열기

	if (!File.is_open())
	{
		UE_LOG("[FObjManager] Failed to open OBJ file: %s\n", FilePathStr.c_str());
		return false;
	}

	FObjParserContext Context{ OutMesh, OutMaterialNames };
	FString Line;

	// 한 줄씩 읽으면서 파싱
	while (std::getline(File, Line))
	{
		if (Line.empty() || Line[0] == '#') continue;

		std::stringstream SS(Line);
		std::string Type;
		SS >> Type;

		// Material
		if (Type == "mtllib")
		{
			std::string MtlFIleName;
			SS >> MtlFIleName;

			FString FullMtlPath = (FPaths::MeshDir() / MtlFIleName).string().c_str();
			ParseMtlFile(FullMtlPath);
		}
		else if (Type == "usemtl") Context.ParseUseMtl(SS);
		else if (Type == "f") Context.ParseFace(SS);
		else if (Type == "v")
		{
			FVector Position;
			SS >> Position.X >> Position.Y >> Position.Z;
			Context.TempPositions.push_back(Position);
		}
		else if (Type == "vt")
		{
			FVector2 UV;
			SS >> UV.X >> UV.Y;
			UV.Y = 1.0f - UV.Y;	// 뒤집기 보정 -> 추후 선택하여 변경하도록 해야할듯.
			Context.TempUVs.push_back(UV);
		}
		else if (Type == "vn")
		{
			FVector Normal;
			SS >> Normal.X >> Normal.Y >> Normal.Z;
			Context.TempNormals.push_back(Normal);
		}
	}

	// Section과 Topology 세팅
	Context.CloseCurrentSection();
	OutMesh->Topology = EMeshTopology::EMT_TriangleList;

	UE_LOG("[FObjManager] Parsed Temporary OBJ: %s (Verts: %zu, Inds: %zu)\n",
		FilePathStr.c_str(), OutMesh->Vertices.size(), OutMesh->Indices.size());

	return true;
}


inline void FObjManager::ClearCache()
{
	for (auto& [PathName, Asset] : ObjStaticMeshMap)
	{
		if (Asset != nullptr)
		{
			if (Asset->GetRenderData())
			{
				delete Asset->GetRenderData();
			}
			delete Asset;
			Asset = nullptr;
		}
	}
	ObjStaticMeshMap.clear();
}

UStaticMesh* FObjManager::GetPrimitiveSphere()
{
	FString SphereKey = "Primitive_Sphere";

	auto It = ObjStaticMeshMap.find(SphereKey);
	if (It != ObjStaticMeshMap.end())
	{
		return It->second;
	}

	FStaticMesh* RawData = new FStaticMesh();
	RawData->PathFileName = SphereKey;

	const int32 Latitudes = 16;  // 위도 분할 (정밀도)
	const int32 Longitudes = 16; // 경도 분할 (정밀도)
	const float Radius = 0.5f;   // 반지름 0.5 (지름 1.0)

	// 1. 정점 생성 (위도/경도 기반)
	for (int32 i = 0; i <= Latitudes; ++i)
	{
		float V = static_cast<float>(i) / static_cast<float>(Latitudes);
		float Phi = V * FMath::PI; // 0 ~ PI

		for (int32 j = 0; j <= Longitudes; ++j)
		{
			float U = static_cast<float>(j) / static_cast<float>(Longitudes);
			float Theta = U * FMath::PI * 2.0f; // 0 ~ 2PI

			float X = Radius * sinf(Phi) * cosf(Theta);
			float Z = Radius * cosf(Phi);
			float Y = Radius * sinf(Phi) * sinf(Theta);

			FVector Pos(X, Y, Z);
			FVector Normal = Pos;
			Normal.Normalize(); // 중심에서 뻗어나가는 방향이 노멀

			// ⭐ 예전 코드에서 가져온 "Normal 기반 색상 매핑" (RGB로 매핑)
			float R = Normal.X * 0.5f + 0.5f;
			float G = Normal.Y * 0.5f + 0.5f;
			float B = Normal.Z * 0.5f + 0.5f;

			FVertex Vert;
			Vert.Position = Pos;
			Vert.Normal = Normal;
			Vert.Color = FVector4(R, G, B, 1.0f); // ⭐ 하얀색 대신 무지개색 적용!
			Vert.UV = FVector2(U, V);

			RawData->Vertices.push_back(Vert);
		}
	}

	// 2. 인덱스 생성 (면 만들기)
	for (int32 i = 0; i < Latitudes; ++i)
	{
		for (int32 j = 0; j < Longitudes; ++j)
		{
			uint32 First = (i * (Longitudes + 1)) + j;
			uint32 Second = First + Longitudes + 1;

			// 첫 번째 삼각형
			RawData->Indices.push_back(First);
			RawData->Indices.push_back(Second);
			RawData->Indices.push_back(First + 1);

			// 두 번째 삼각형
			RawData->Indices.push_back(Second);
			RawData->Indices.push_back(Second + 1);
			RawData->Indices.push_back(First + 1);
		}
	}

	RawData->Topology = EMeshTopology::EMT_TriangleList;

	FMeshSection DefaultSection;
	DefaultSection.MaterialIndex = 0;
	DefaultSection.StartIndex = 0;
	DefaultSection.IndexCount = static_cast<uint32>(RawData->Indices.size());
	RawData->Sections.push_back(DefaultSection);

	RawData->UpdateLocalBound();

	UStaticMesh* SphereAsset = new UStaticMesh();
	SphereAsset->SetStaticMeshAsset(RawData);

	SphereAsset->LocalBounds.Radius = RawData->GetLocalBoundRadius();
	SphereAsset->LocalBounds.Center = RawData->GetCenterCoord();
	SphereAsset->LocalBounds.BoxExtent = (RawData->GetMaxCoord() - RawData->GetMinCoord()) * 0.5f;

	ObjStaticMeshMap[SphereKey] = SphereAsset;

	return SphereAsset;
}

UStaticMesh* FObjManager::GetPrimitiveSky()
{
	FString SkyKey = "Primitive_Sky";

	auto It = ObjStaticMeshMap.find(SkyKey);
	if (It != ObjStaticMeshMap.end())
	{
		return It->second;
	}

	FStaticMesh* RawData = new FStaticMesh();
	RawData->PathFileName = SkyKey;

	const int32 Segments = 32;
	const int32 Rings = 32;
	const float Radius = 0.5f;

	// 1. 정점 생성 (안쪽을 바라보는 노멀)
	for (int32 Ring = 0; Ring <= Rings; ++Ring)
	{
		float Phi = FMath::PI * static_cast<float>(Ring) / static_cast<float>(Rings);
		float Z = cosf(Phi);
		float SinPhi = sinf(Phi);

		for (int32 Seg = 0; Seg <= Segments; ++Seg)
		{
			float Theta = FMath::TwoPi * static_cast<float>(Seg) / static_cast<float>(Segments);
			float X = SinPhi * cosf(Theta);
			float Y = SinPhi * sinf(Theta);

			FVector Pos(X * Radius, Y * Radius, Z * Radius);

			// ⭐ 핵심: 하늘은 안에서 밖을 보므로 노멀을 뒤집어 줍니다!
			FVector Normal(-X, -Y, -Z);
			Normal.Normalize();

			FVertex Vert;
			Vert.Position = Pos;
			Vert.Normal = Normal;
			Vert.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f); // 일단 하얀색

			// UV 좌표도 꼼꼼히 챙겨줍니다 (나중에 구름 텍스처 발라야 하니까요!)
			Vert.UV = FVector2(static_cast<float>(Seg) / Segments, static_cast<float>(Ring) / Rings);

			RawData->Vertices.push_back(Vert);
		}
	}

	// 2. 인덱스 생성 (면 그리는 순서 뒤집기)
	for (int32 Ring = 0; Ring < Rings; ++Ring)
	{
		for (int32 Seg = 0; Seg < Segments; ++Seg)
		{
			uint32 Current = Ring * (Segments + 1) + Seg;
			uint32 Next = Current + Segments + 1;

			// ⭐ 핵심: 컬링되지 않도록 삼각형 그리는 순서를 바꿨습니다 (올려주신 기존 로직 유지)
			RawData->Indices.push_back(Current);
			RawData->Indices.push_back(Current + 1);
			RawData->Indices.push_back(Next);

			RawData->Indices.push_back(Current + 1);
			RawData->Indices.push_back(Next + 1);
			RawData->Indices.push_back(Next);
		}
	}

	RawData->Topology = EMeshTopology::EMT_TriangleList;

	FMeshSection DefaultSection;
	DefaultSection.MaterialIndex = 0;
	DefaultSection.StartIndex = 0;
	DefaultSection.IndexCount = static_cast<uint32>(RawData->Indices.size());
	RawData->Sections.push_back(DefaultSection);

	RawData->UpdateLocalBound();

	UStaticMesh* SkyAsset = new UStaticMesh();
	SkyAsset->SetStaticMeshAsset(RawData);

	SkyAsset->LocalBounds.Radius = RawData->GetLocalBoundRadius();
	SkyAsset->LocalBounds.Center = RawData->GetCenterCoord();
	SkyAsset->LocalBounds.BoxExtent = (RawData->GetMaxCoord() - RawData->GetMinCoord()) * 0.5f;

	ObjStaticMeshMap[SkyKey] = SkyAsset;

	return SkyAsset;
}