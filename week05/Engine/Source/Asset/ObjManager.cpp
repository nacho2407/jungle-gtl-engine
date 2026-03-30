#include "ObjManager.h"
#include "Core/Paths.h"
#include <fstream>
#include <sstream>

#include "Debug/EngineLog.h"
#include "Renderer/Renderer.h"
#include "Math/MathUtility.h"
#include "Renderer/MaterialManager.h"

TMap<FString, UStaticMesh*> FObjManager::ObjStaticMeshMap;

inline UStaticMesh* FObjManager::LoadObjStaticMeshAsset(const FString& PathFileName)
{
	// 추후에 obj파싱이 끝나면 없앨 코드
	if (PathFileName == "Primitive_Cube")  return GetPrimitiveCube();
	if (PathFileName == "Primitive_Plane") return GetPrimitivePlane();
	if (PathFileName == "Primitive_Sphere") return GetPrimitiveSphere();

	// ---------------------------------------------
	auto It = ObjStaticMeshMap.find(PathFileName);
	if (It != ObjStaticMeshMap.end())
	{
		return It->second;
	}

	FStaticMesh* RawData = new FStaticMesh();
	RawData->PathFileName = PathFileName;

	if (!ParseObjFile(PathFileName, RawData))
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

	ObjStaticMeshMap[PathFileName] = NewAsset;

	return NewAsset;
}

inline bool FObjManager::ParseObjFile(const FString& FilePath, FStaticMesh* OutMesh)
{
	/*// TODO: 나중에 .obj 파서로 교체할 부분
	// 지금은 기존의 바이너리 포맷 읽기 로직을 임시로 사용합니다.

	FString FilePathStr(FilePath.c_str());
	std::ifstream File(FilePathStr, std::ios::binary);

	if (!File.is_open())
	{
		printf("[FObjManager] Failed to open temporary binary file: %s\n", FilePathStr.c_str());
		return false;
	}

	uint32 VertexCount = 0;
	File.read(reinterpret_cast<char*>(&VertexCount), sizeof(uint32));
	OutMesh->Vertices.resize(VertexCount);
	File.read(reinterpret_cast<char*>(OutMesh->Vertices.data()), VertexCount * sizeof(FStaticVertex));

	uint32 IndexCount = 0;
	File.read(reinterpret_cast<char*>(&IndexCount), sizeof(uint32));
	OutMesh->Indices.resize(IndexCount);
	File.read(reinterpret_cast<char*>(OutMesh->Indices.data()), IndexCount * sizeof(uint32));

	// 이전에 없던 section 추가 - 억지로 만들었음.
	FMeshSection DefaultSection;
	DefaultSection.MaterialIndex = 0; // 0번 머티리얼 사용
	DefaultSection.StartIndex = 0;    // 처음부터
	DefaultSection.IndexCount = IndexCount; // 끝까지 전부 그리기
	OutMesh->Sections.push_back(DefaultSection);

	OutMesh->Topology = EMeshTopology::EMT_TriangleList;

	if (File.fail())
	{
		printf("[FObjManager] Failed to read temporary binary file: %s\n", FilePathStr.c_str());
		return false;
	}

	printf("[FObjManager] Loaded temporary binary mesh: %s (Vertices: %u, Indices: %u)\n",
		FilePathStr.c_str(), VertexCount, IndexCount);

	return true;*/

	// 1. 파일 열기 (엔진의 FString이나 FPaths에 맞춰서 수정하세요)
	std::string FilePathStr(FilePath.c_str());
	// std::string FilePathStr(FPaths::ToAbsolutePath(FilePath).c_str()); 

	std::ifstream File(FilePathStr); // 텍스트 모드로 열기
	if (!File.is_open())
	{
		printf("[FObjManager] Failed to open OBJ file: %s\n", FilePathStr.c_str());
		return false;
	}

	std::vector<FVector> TempPositions;
	std::vector<FVector2> TempUVs;

	std::string Line;

	// 2. 한 줄씩 읽으면서 파싱
	while (std::getline(File, Line))
	{
		std::stringstream SS(Line);
		std::string Type;
		SS >> Type;

		// =========================
		// Vertex Position (v)
		// =========================
		if (Type == "v")
		{
			FVector Pos;
			SS >> Pos.X >> Pos.Y >> Pos.Z;
			TempPositions.push_back(Pos);
		}
		// =========================
		// UV (vt)
		// =========================
		else if (Type == "vt")
		{
			FVector2 UV;
			SS >> UV.X >> UV.Y;

			// OBJ는 V좌표가 뒤집혀 있는 경우가 많아 보정
			UV.Y = 1.0f - UV.Y;
			TempUVs.push_back(UV);
		}
		// =========================
		// Face (f) 조립
		// =========================
		else if (Type == "f")
		{
			std::string VStr;

			struct FIndex
			{
				uint32 PosIdx;
				uint32 UVIdx;
			};

			std::vector<FIndex> Face;

			while (SS >> VStr)
			{
				std::stringstream VSS(VStr);
				std::string PosStr, UVStr;

				std::getline(VSS, PosStr, '/');
				std::getline(VSS, UVStr, '/'); // 노멀(vn)은 당장 무시

				FIndex Idx{};
				Idx.PosIdx = std::stoi(PosStr) - 1;

				if (!UVStr.empty())
					Idx.UVIdx = std::stoi(UVStr) - 1;
				else
					Idx.UVIdx = 0;

				Face.push_back(Idx);
			}

			// =========================
			// Fan Triangulation (다각형을 삼각형으로 쪼개기)
			// =========================
			for (size_t i = 1; i + 1 < Face.size(); ++i)
			{
				FIndex I0 = Face[0];
				FIndex I1 = Face[i];
				FIndex I2 = Face[i + 1];

				uint32 BaseIndex = static_cast<uint32>(OutMesh->Vertices.size());

				auto AddVertex = [&](const FIndex& Idx)
					{
						FVertex V{}; // ⭐ FPrimitiveVertex -> FStaticVertex로 변경!

						V.Position = TempPositions[Idx.PosIdx];

						// 임시 컬러값 세팅 (기존 코드 유지)
						V.Color = FVector4(V.Position.X, V.Position.Y, V.Position.Z, 1.0f);

						// UV 세팅 (인덱스 범위 체크 안전빵 추가)
						if (!TempUVs.empty() && Idx.UVIdx < TempUVs.size())
							V.UV = TempUVs[Idx.UVIdx];
						else
							V.UV = FVector2(0, 0);

						// V.Normal = FVector(0,0,0); // 노멀은 일단 0으로 둠

						OutMesh->Vertices.push_back(V);
					};

				AddVertex(I0);
				AddVertex(I1);
				AddVertex(I2);

				OutMesh->Indices.push_back(BaseIndex + 0);
				OutMesh->Indices.push_back(BaseIndex + 1);
				OutMesh->Indices.push_back(BaseIndex + 2);
			}
		}
	}

	// =========================
	// ⭐ 마무리: Section과 Topology 세팅
	// =========================

	// UStaticMeshComponent가 머티리얼 슬롯을 만들고 렌더러가 그릴 수 있도록
	// 전체 인덱스를 덮는 단일 섹션(0번 재질)을 만들어 줍니다.
	FMeshSection DefaultSection;
	DefaultSection.MaterialIndex = 0;
	DefaultSection.StartIndex = 0;
	DefaultSection.IndexCount = static_cast<uint32>(OutMesh->Indices.size());

	OutMesh->Sections.push_back(DefaultSection);
	OutMesh->Topology = EMeshTopology::EMT_TriangleList;

	printf("[FObjManager] Parsed Temporary OBJ: %s (Verts: %zu, Inds: %zu)\n",
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

UStaticMesh* FObjManager::GetPrimitivePlane()
{
	FString PlaneKey = "Primitive_Plane";

	auto It = ObjStaticMeshMap.find(PlaneKey);
	if (It != ObjStaticMeshMap.end())
	{
		return It->second;
	}

	FStaticMesh* RawData = new FStaticMesh();
	RawData->PathFileName = PlaneKey;

	FVector4 White = { 1.0f, 1.0f, 1.0f, 1.0f };
	FVector Normal = { 0.0f, 1.0f, 0.0f };

	RawData->Vertices.push_back({ { -5.0f,  5.0f, 0.0f }, White, Normal, {0.0f, 0.0f} });
	RawData->Vertices.push_back({ {  5.0f,  5.0f, 0.0f }, White, Normal, {1.0f, 0.0f} });
	RawData->Vertices.push_back({ {  5.0f, -5.0f, 0.0f }, White, Normal, {1.0f, 1.0f} });
	RawData->Vertices.push_back({ { -5.0f, -5.0f, 0.0f }, White, Normal, {0.0f, 1.0f} });

	RawData->Indices.push_back(0);
	RawData->Indices.push_back(2);
	RawData->Indices.push_back(1);
	RawData->Indices.push_back(0);
	RawData->Indices.push_back(3);
	RawData->Indices.push_back(2);

	RawData->Topology = EMeshTopology::EMT_TriangleList;

	FMeshSection DefaultSection;
	DefaultSection.MaterialIndex = 0;
	DefaultSection.StartIndex = 0;
	DefaultSection.IndexCount = static_cast<uint32>(RawData->Indices.size());
	RawData->Sections.push_back(DefaultSection);

	RawData->UpdateLocalBound();

	UStaticMesh* PlaneAsset = new UStaticMesh();
	PlaneAsset->SetStaticMeshAsset(RawData);

	PlaneAsset->LocalBounds.Radius = RawData->GetLocalBoundRadius();
	PlaneAsset->LocalBounds.Center = RawData->GetCenterCoord();
	PlaneAsset->LocalBounds.BoxExtent = (RawData->GetMaxCoord() - RawData->GetMinCoord()) * 0.5f;

	ObjStaticMeshMap[PlaneKey] = PlaneAsset;

	return PlaneAsset;
}

UStaticMesh* FObjManager::GetPrimitiveCube()
{
	FString CubeKey = "Primitive_Cube";

	auto It = ObjStaticMeshMap.find(CubeKey);
	if (It != ObjStaticMeshMap.end())
	{
		return It->second;
	}

	FStaticMesh* RawData = new FStaticMesh();
	RawData->PathFileName = CubeKey;

	FVector4 Red = { 1.0f, 0.3f, 0.3f, 1.0f };
	FVector4 Green = { 0.3f, 1.0f, 0.3f, 1.0f };
	FVector4 Blue = { 0.3f, 0.3f, 1.0f, 1.0f };
	FVector4 Yellow = { 1.0f, 1.0f, 0.3f, 1.0f };
	FVector4 Cyan = { 0.3f, 1.0f, 1.0f, 1.0f };
	FVector4 Magenta = { 1.0f, 0.3f, 1.0f, 1.0f };

	RawData->Vertices.push_back({ {  0.5f, -0.5f, -0.5f }, Red, {  1.0f,  0.0f,  0.0f }, {0.0f, 0.0f} });
	RawData->Vertices.push_back({ {  0.5f,  0.5f, -0.5f }, Red, {  1.0f,  0.0f,  0.0f }, {1.0f, 0.0f} });
	RawData->Vertices.push_back({ {  0.5f,  0.5f,  0.5f }, Red, {  1.0f,  0.0f,  0.0f }, {1.0f, 1.0f} });
	RawData->Vertices.push_back({ {  0.5f, -0.5f,  0.5f }, Red, {  1.0f,  0.0f,  0.0f }, {0.0f, 1.0f} });

	// Back face (x = -0.5) — Green
	RawData->Vertices.push_back({ { -0.5f,  0.5f, -0.5f }, Green, { -1.0f,  0.0f,  0.0f }, {0.0f, 0.0f} });
	RawData->Vertices.push_back({ { -0.5f, -0.5f, -0.5f }, Green, { -1.0f,  0.0f,  0.0f }, {1.0f, 0.0f} });
	RawData->Vertices.push_back({ { -0.5f, -0.5f,  0.5f }, Green, { -1.0f,  0.0f,  0.0f }, {1.0f, 1.0f} });
	RawData->Vertices.push_back({ { -0.5f,  0.5f,  0.5f }, Green, { -1.0f,  0.0f,  0.0f }, {0.0f, 1.0f} });

	// Top face (z = +0.5) — Blue
	RawData->Vertices.push_back({ { -0.5f, -0.5f,  0.5f }, Blue, {  0.0f,  0.0f,  1.0f }, {0.0f, 0.0f} });
	RawData->Vertices.push_back({ {  0.5f, -0.5f,  0.5f }, Blue, {  0.0f,  0.0f,  1.0f }, {1.0f, 0.0f} });
	RawData->Vertices.push_back({ {  0.5f,  0.5f,  0.5f }, Blue, {  0.0f,  0.0f,  1.0f }, {1.0f, 1.0f} });
	RawData->Vertices.push_back({ { -0.5f,  0.5f,  0.5f }, Blue, {  0.0f,  0.0f,  1.0f }, {0.0f, 1.0f} });

	// Bottom face (z = -0.5) — Yellow
	RawData->Vertices.push_back({ { -0.5f,  0.5f, -0.5f }, Yellow, {  0.0f,  0.0f, -1.0f }, {0.0f, 0.0f} });
	RawData->Vertices.push_back({ {  0.5f,  0.5f, -0.5f }, Yellow, {  0.0f,  0.0f, -1.0f }, {1.0f, 0.0f} });
	RawData->Vertices.push_back({ {  0.5f, -0.5f, -0.5f }, Yellow, {  0.0f,  0.0f, -1.0f }, {1.0f, 1.0f} });
	RawData->Vertices.push_back({ { -0.5f, -0.5f, -0.5f }, Yellow, {  0.0f,  0.0f, -1.0f }, {0.0f, 1.0f} });

	// Right face (y = +0.5) — Cyan
	RawData->Vertices.push_back({ {  0.5f,  0.5f, -0.5f }, Cyan, {  0.0f,  1.0f,  0.0f }, {0.0f, 0.0f} });
	RawData->Vertices.push_back({ { -0.5f,  0.5f, -0.5f }, Cyan, {  0.0f,  1.0f,  0.0f }, {1.0f, 0.0f} });
	RawData->Vertices.push_back({ { -0.5f,  0.5f,  0.5f }, Cyan, {  0.0f,  1.0f,  0.0f }, {1.0f, 1.0f} });
	RawData->Vertices.push_back({ {  0.5f,  0.5f,  0.5f }, Cyan, {  0.0f,  1.0f,  0.0f }, {0.0f, 1.0f} });

	// Left face (y = -0.5) — Magenta
	RawData->Vertices.push_back({ { -0.5f, -0.5f, -0.5f }, Magenta, {  0.0f, -1.0f,  0.0f }, {0.0f, 0.0f} });
	RawData->Vertices.push_back({ {  0.5f, -0.5f, -0.5f }, Magenta, {  0.0f, -1.0f,  0.0f }, {1.0f, 0.0f} });
	RawData->Vertices.push_back({ {  0.5f, -0.5f,  0.5f }, Magenta, {  0.0f, -1.0f,  0.0f }, {1.0f, 1.0f} });
	RawData->Vertices.push_back({ { -0.5f, -0.5f,  0.5f }, Magenta, {  0.0f, -1.0f,  0.0f }, {0.0f, 1.0f} });

	// 36 indices (6 faces * 2 triangles * 3 vertices)
	for (uint32 i = 0; i < 6; ++i)
	{
		uint32 Base = i * 4;
		RawData->Indices.push_back(Base + 0);
		RawData->Indices.push_back(Base + 1);
		RawData->Indices.push_back(Base + 2);
		RawData->Indices.push_back(Base + 0);
		RawData->Indices.push_back(Base + 2);
		RawData->Indices.push_back(Base + 3);
	}

	RawData->Topology = EMeshTopology::EMT_TriangleList;

	FMeshSection Section0;
	Section0.MaterialIndex = 0;
	Section0.StartIndex = 0;
	Section0.IndexCount = 18;
	RawData->Sections.push_back(Section0);

	// 1번 섹션: 뒤쪽 18개 인덱스 (면 3개)
	FMeshSection Section1;
	Section1.MaterialIndex = 1;
	Section1.StartIndex = 18;
	Section1.IndexCount = 18;
	RawData->Sections.push_back(Section1);

	RawData->UpdateLocalBound();

	UStaticMesh* CubeAsset = new UStaticMesh();
	CubeAsset->SetStaticMeshAsset(RawData);

	CubeAsset->LocalBounds.Radius = RawData->GetLocalBoundRadius();
	CubeAsset->LocalBounds.Center = RawData->GetCenterCoord();
	CubeAsset->LocalBounds.BoxExtent = (RawData->GetMaxCoord() - RawData->GetMinCoord()) * 0.5f;

	// ==========================================================
	// ⭐ UStaticMesh(원본 에셋)에 2개의 기본 머티리얼을 꽂아줍니다!
	// ==========================================================
	// (주의: "M_Default"와 "M_Font"는 엔진에 미리 로드되어 있다고 가정한 이름입니다. 
	// 실제 로드되어 있는 아무 머티리얼 이름 2개로 바꿔주세요)

	/*auto Mat0 = FMaterialManager::Get().FindByName("M_Default");
	auto Mat1 = FMaterialManager::Get().FindByName("M_Default"); // 아까 초기화 코드에 있던 다른 머티리얼

	if (!Mat0) UE_LOG("[경고] M_Default 머티리얼을 찾을 수 없습니다! (초기화 순서 의심)");
	if (!Mat1) UE_LOG("[경고] M_Default_Texture 머티리얼을 찾을 수 없습니다!");

	CubeAsset->AddDefaultMaterial(Mat0);
	CubeAsset->AddDefaultMaterial(Mat1);*/

	ObjStaticMeshMap[CubeKey] = CubeAsset;

	return CubeAsset;

	/*FMeshSection DefaultSection;
	DefaultSection.MaterialIndex = 0;
	DefaultSection.StartIndex = 0;
	DefaultSection.IndexCount = static_cast<uint32>(RawData->Indices.size());
	RawData->Sections.push_back(DefaultSection);

	RawData->UpdateLocalBound();

	UStaticMesh* CubeAsset = new UStaticMesh();
	CubeAsset->SetStaticMeshAsset(RawData);

	CubeAsset->LocalBounds.Radius = RawData->GetLocalBoundRadius();
	CubeAsset->LocalBounds.Center = RawData->GetCenterCoord();
	CubeAsset->LocalBounds.BoxExtent = (RawData->GetMaxCoord() - RawData->GetMinCoord()) * 0.5f;

	ObjStaticMeshMap[CubeKey] = CubeAsset;

	return CubeAsset;*/
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