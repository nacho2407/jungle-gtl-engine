#include "ObjManager.h"
#include "Core/Paths.h"
#include <filesystem>
#include <fstream>
#include <sstream>

#include "Core/Engine.h"
#include "Debug/EngineLog.h"
#include "Renderer/Renderer.h"
#include "Math/MathUtility.h"
#include "Renderer/MaterialManager.h"
#include "Renderer/Shader.h"
#include <map>
#include "Renderer/Material.h"
#include "Renderer/ShaderMap.h"

TMap<FString, UStaticMesh*> FObjManager::ObjStaticMeshMap;

namespace
{
	struct FObjParserContext
	{
		FStaticMesh* OutMesh;
		TArray<FString>& OutMaterialNames;

		TArray<FVector> TempPositions;
		TArray<FVector2> TempUVs;
		TArray<FVector> TempNormals;

		struct FIndex
		{
			uint32 PositionIndex;
			uint32 UVIndex;
			uint32 NormalIndex;

			bool operator<(const FIndex& Other) const
			{
				if (PositionIndex != Other.PositionIndex) return PositionIndex < Other.PositionIndex;
				if (UVIndex != Other.UVIndex) return UVIndex < Other.UVIndex;
				return NormalIndex < Other.NormalIndex;
			}
		};

		std::map<FIndex, uint32> VertexCache;

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
			CurrentMaterialIndex = static_cast<int32>(OutMaterialNames.size());
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
				Idx.UVIndex = UVString.empty() ? -1 : std::stoi(UVString) - 1;
				Idx.NormalIndex = NormalString.empty() ? -1 : std::stoi(NormalString) - 1;

				Face.push_back(Idx);
			}

			TArray<uint32> FaceIndices;

			for (const FIndex& Idx : Face)
			{
				auto It = VertexCache.find(Idx);
				if (It != VertexCache.end())
				{
					FaceIndices.push_back(It->second);
				}
				else
				{
					uint32 NewVertexIndex = static_cast<uint32>(OutMesh->Vertices.size());

					FVertex V{};
					V.Position = TempPositions[Idx.PositionIndex];
					V.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

					if (!TempUVs.empty() && Idx.UVIndex < TempUVs.size())
					{
						V.UV = TempUVs[Idx.UVIndex];
					}
					if (!TempNormals.empty() && Idx.NormalIndex < TempNormals.size())
					{
						V.Normal = TempNormals[Idx.NormalIndex];
					}

					OutMesh->Vertices.push_back(V);

					VertexCache[Idx] = NewVertexIndex;
					FaceIndices.push_back(NewVertexIndex);
				}
			}

			for (size_t i = 1; i + 1 < FaceIndices.size(); ++i)
			{
				OutMesh->Indices.push_back(FaceIndices[0]);
				OutMesh->Indices.push_back(FaceIndices[i + 1]);
				OutMesh->Indices.push_back(FaceIndices[i]);
			}
		}
	};
}

inline UStaticMesh* FObjManager::LoadObjStaticMeshAsset(const FString& PathFileName)
{
	auto It = ObjStaticMeshMap.find(PathFileName);
	if (It != ObjStaticMeshMap.end())
	{
		return It->second;
	}

	auto RawData = std::make_unique<FStaticMesh>();
	RawData->PathFileName = PathFileName;

	TArray<FString> FoundMaterials;
	if (!ParseObjFile(PathFileName, RawData.get(), FoundMaterials))
	{
		return nullptr;
	}

	RawData->UpdateLocalBound();

	UStaticMesh* NewAsset = new UStaticMesh();
	NewAsset->SetStaticMeshAsset(RawData.release());

	NewAsset->LocalBounds.Radius = NewAsset->GetRenderData()->GetLocalBoundRadius();
	NewAsset->LocalBounds.Center = NewAsset->GetRenderData()->GetCenterCoord();
	NewAsset->LocalBounds.BoxExtent = (NewAsset->GetRenderData()->GetMaxCoord() - NewAsset->GetRenderData()->GetMinCoord()) * 0.5f;

	for (const FString& MatName : FoundMaterials)
	{
		auto Material = FMaterialManager::Get().FindByName(MatName);

		if (!Material)
		{
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

bool FObjManager::ParseMtlFile(const FString& MtlFIlePath)
{
	const FString AbsolutePath = FPaths::ToAbsolutePath(MtlFIlePath);
	const std::filesystem::path FilePath = std::filesystem::path(FPaths::ToWide(AbsolutePath)).lexically_normal();

	std::ifstream File(FilePath);
	if (!File.is_open())
	{
		return false;
	}

	std::string Line;
	std::shared_ptr<FMaterial> CurrentMaterial = nullptr;

	while (std::getline(File, Line))
	{
		if (Line.empty() || Line[0] == '#')
		{
			continue;
		}

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
			std::wstring PSPath = FPaths::ShaderDir() / L"ColorPixelShader.hlsl";
			CurrentMaterial->SetVertexShader(FShaderMap::Get().GetOrCreateVertexShader(GEngine->GetRenderer()->GetDevice(), VSPath.c_str()));
			CurrentMaterial->SetPixelShader(FShaderMap::Get().GetOrCreatePixelShader(GEngine->GetRenderer()->GetDevice(), PSPath.c_str()));

			auto DefaultTexMat = GEngine->GetRenderer()->GetDefaultTextureMaterial();
			CurrentMaterial->SetRasterizerOption(DefaultTexMat->GetRasterizerOption());
			CurrentMaterial->SetRasterizerState(DefaultTexMat->GetRasterizerState());
			CurrentMaterial->SetDepthStencilOption(DefaultTexMat->GetDepthStencilOption());
			CurrentMaterial->SetDepthStencilState(DefaultTexMat->GetDepthStencilState());

			int32 SlotIndex = CurrentMaterial->CreateConstantBuffer(GEngine->GetRenderer()->GetDevice(), 32);
			if (SlotIndex >= 0)
			{
				CurrentMaterial->RegisterParameter("BaseColor", SlotIndex, 0, 16);
				float White[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
				CurrentMaterial->GetConstantBuffer(SlotIndex)->SetData(White, sizeof(White));

				CurrentMaterial->RegisterParameter("UVScrollSpeed", SlotIndex, 16, 16);
				float DefaultScroll[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				CurrentMaterial->GetConstantBuffer(SlotIndex)->SetData(DefaultScroll, sizeof(DefaultScroll), 16);
			}

			FMaterialManager::Get().Register(MaterialName.c_str(), CurrentMaterial);
		}
		else if (Type == "Kd" && CurrentMaterial)
		{
			float R, G, B;
			SS >> R >> G >> B;

			float DiffuseColor[4] = { R, G, B, 1.0f };

			auto CB = CurrentMaterial->GetConstantBuffer(0);
			if (CB)
			{
				CB->SetData(DiffuseColor, sizeof(DiffuseColor));
			}
		}
		else if (Type == "map_Kd" && CurrentMaterial)
		{
			std::string TextureFileName;
			SS >> TextureFileName;

			std::filesystem::path TexturePath = FPaths::TextureDir() / TextureFileName;

			ID3D11ShaderResourceView* NewSRV = nullptr;
			if (GEngine->GetRenderer()->CreateTextureFromSTB(GEngine->GetRenderer()->GetDevice(), TexturePath.string().c_str(), &NewSRV))
			{
				auto MaterialTexture = std::make_shared<FMaterialTexture>();
				MaterialTexture->TextureSRV = NewSRV;
				CurrentMaterial->SetMaterialTexture(MaterialTexture);

				std::wstring TexPSPath = FPaths::ShaderDir() / L"TexturePixelShader.hlsl";
				CurrentMaterial->SetPixelShader(FShaderMap::Get().GetOrCreatePixelShader(GEngine->GetRenderer()->GetDevice(), TexPSPath.c_str()));

				std::wstring TexVSPath = FPaths::ShaderDir() / L"TextureVertexShader.hlsl";
				CurrentMaterial->SetVertexShader(FShaderMap::Get().GetOrCreateVertexShader(GEngine->GetRenderer()->GetDevice(), TexVSPath.c_str()));

				UE_LOG("[MTL 파서] %s 텍스처 자동 로드 및 장착 완료!", TexPSPath.c_str());
			}
		}
	}

	return true;
}

void FObjManager::PreloadAllObjFiles(const FString& DirectoryPath)
{
	const FString AbsolutePath = FPaths::ToAbsolutePath(DirectoryPath);
	const std::filesystem::path DirPath = std::filesystem::path(FPaths::ToWide(AbsolutePath)).lexically_normal();

	// 폴더가 존재하는지 확인
	if (!std::filesystem::exists(DirPath) || !std::filesystem::is_directory(DirPath))
	{
		UE_LOG("[FObjManager] Preload 실패: 폴더를 찾을 수 없습니다. (%s)", AbsolutePath.c_str());
		return;
	}

	for (const auto& Entry : std::filesystem::directory_iterator(DirPath))
	{
		if (Entry.is_regular_file() && Entry.path().extension() == ".obj")
		{
			std::string FullFilePath = Entry.path().string();

			UStaticMesh* LoadedMesh = LoadObjStaticMeshAsset(FullFilePath.c_str());
		}
	}
}

inline bool FObjManager::ParseObjFile(const FString& FilePath, FStaticMesh* OutMesh, TArray<FString>& OutMaterialNames)
{
	const FString AbsolutePath = FPaths::ToAbsolutePath(FilePath);
	const std::filesystem::path ObjPath = std::filesystem::path(FPaths::ToWide(AbsolutePath)).lexically_normal();
	const std::string FilePathDisplay = AbsolutePath;

	std::ifstream File(ObjPath);
	if (!File.is_open())
	{
		UE_LOG("[FObjManager] Failed to open OBJ file: %s\n", FilePathDisplay.c_str());
		return false;
	}

	FObjParserContext Context{ OutMesh, OutMaterialNames };
	std::string Line;

	while (std::getline(File, Line))
	{
		if (Line.empty() || Line[0] == '#')
		{
			continue;
		}

		std::stringstream SS(Line);
		std::string Type;
		SS >> Type;

		if (Type == "mtllib")
		{
			std::string MtlFIleName;
			SS >> MtlFIleName;

			std::filesystem::path ResolvedMtlPath = ObjPath.parent_path() / MtlFIleName;
			if (!std::filesystem::exists(ResolvedMtlPath))
			{
				ResolvedMtlPath = FPaths::MaterialDir() / MtlFIleName;
			}

			ParseMtlFile(ResolvedMtlPath.string().c_str());
		}
		else if (Type == "usemtl")
		{
			Context.ParseUseMtl(SS);
		}
		else if (Type == "f")
		{
			Context.ParseFace(SS);
		}
		else if (Type == "v")
		{
			FVector Position;
			SS >> Position.X >> Position.Y >> Position.Z;
			Position.Y = -Position.Y;
			Context.TempPositions.push_back(Position);
		}
		else if (Type == "vt")
		{
			FVector2 UV;
			SS >> UV.X >> UV.Y;
			UV.Y = 1.0f - UV.Y;
			Context.TempUVs.push_back(UV);
		}
		else if (Type == "vn")
		{
			FVector Normal;
			SS >> Normal.X >> Normal.Y >> Normal.Z;
			Context.TempNormals.push_back(Normal);
		}
	}

	Context.CloseCurrentSection();
	OutMesh->Topology = EMeshTopology::EMT_TriangleList;

	UE_LOG("[FObjManager] Parsed Temporary OBJ: %s (Verts: %zu, Inds: %zu)\n",
		FilePathDisplay.c_str(), OutMesh->Vertices.size(), OutMesh->Indices.size());

	return true;
}

inline void FObjManager::ClearCache()
{
	for (auto& [PathName, Asset] : ObjStaticMeshMap)
	{
		if (Asset != nullptr)
		{
			delete Asset;
			Asset = nullptr;
		}
	}

	ObjStaticMeshMap.clear();
}
