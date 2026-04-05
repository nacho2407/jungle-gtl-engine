#include "ObjManager.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <Windows.h>

#include "Core/Engine.h"
#include "Asset/StaticMeshLODBuilder.h"
#include "Core/Paths.h"
#include "Debug/EngineLog.h"
#include "Math/MathUtility.h"
#include <map>

#include "Object/ObjectFactory.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialManager.h"
#include "Renderer/Renderer.h"
#include "Renderer/Shader.h"
#include "Renderer/ShaderMap.h"

TMap<FString, UStaticMesh*> FObjManager::ObjStaticMeshMap;

namespace
{
	constexpr char GModelMagic[4] = { 'M', 'O', 'D', 'L' };
	constexpr uint32 GModelVersionLegacy = 1;
	constexpr uint32 GModelVersionEmbeddedMaterials = 2;
	constexpr uint32 GModelVersion = GModelVersionEmbeddedMaterials;

	constexpr char GLODMagic[4] = { 'L', 'O', 'D', 'F' };
	constexpr uint32 GLODVersion = 1;

	FString GetLodFilePath(const FString& MeshPathFileName, int32 LodLevel)
	{
		const std::filesystem::path MeshPath = FPaths::ToPath(FPaths::ToAbsolutePath(MeshPathFileName)).lexically_normal();
		const std::filesystem::path LodPath = MeshPath.parent_path()
			/ (MeshPath.stem().string() + "_lod" + std::to_string(LodLevel) + ".lod");
		FString Result = LodPath.string();
		std::replace(Result.begin(), Result.end(), '\\', '/');
		return Result;
	}

	void BuildOrLoadLODs(UStaticMesh& Asset, const FString& PathFileName)
	{
		const FStaticMeshLODSettings Settings;

		bool bAllCached = true;
		for (int32 i = 1; i <= Settings.NumLODs; ++i)
		{
			const FString LodPath = GetLodFilePath(PathFileName, i);
			if (!std::filesystem::exists(FPaths::ToPath(LodPath)))
			{
				bAllCached = false;
				break;
			}
		}

		if (bAllCached)
		{
			for (int32 i = 1; i <= Settings.NumLODs; ++i)
			{
				const FString LodPath = GetLodFilePath(PathFileName, i);
				FStaticMesh* LodMesh = FObjManager::LoadLODAsset(LodPath);
				if (LodMesh)
				{
					Asset.AddLOD(std::unique_ptr<FStaticMesh>(LodMesh), Settings.DistanceStep * static_cast<float>(i));
				}
			}
		}
		else
		{
			FStaticMeshLODBuilder::BuildLODs(Asset, Settings);

			const uint32 LodCount = Asset.GetLODCount();
			for (uint32 i = 1; i < LodCount; ++i)
			{
				FStaticMesh* LodMesh = Asset.GetRenderData(static_cast<int32>(i));
				if (LodMesh)
				{
					const FString LodPath = GetLodFilePath(PathFileName, static_cast<int32>(i));
					FObjManager::SaveLODAsset(LodPath, *LodMesh);
				}
			}
		}
	}

	FString NormalizeSlashes(FString Path)
	{
		std::replace(Path.begin(), Path.end(), '\\', '/');
		return Path;
	}
	FString GetStandardizedMeshPath(const FString& InPath)
	{
		FString Path = NormalizeSlashes(InPath);
		if (Path.starts_with("Data/"))
		{
			Path = "Assets/Meshes/" + Path;
		}
		else if (Path.find('/') == std::string::npos)
		{
			Path = "Assets/Meshes/" + Path;
		}
		Path = FPaths::ToRelativePath(Path);

		return NormalizeSlashes(Path);
	}

	FString BuildObjCacheKey(const FString& PathFileName, const FObjLoadOptions& LoadOptions)
	{
		const FString StandardizedPath = GetStandardizedMeshPath(PathFileName);
		if (LoadOptions.bUseLegacyObjConversion)
		{
			return StandardizedPath + "|OBJ|LEGACY";
		}

		auto AxisToken = [](EObjImportAxis Axis) -> const char*
		{
			switch (Axis)
			{
			case EObjImportAxis::PosX: return "+X";
			case EObjImportAxis::NegX: return "-X";
			case EObjImportAxis::PosY: return "+Y";
			case EObjImportAxis::NegY: return "-Y";
			case EObjImportAxis::PosZ: return "+Z";
			case EObjImportAxis::NegZ: return "-Z";
			default: return "+X";
			}
		};

		return StandardizedPath + "|OBJ|F=" + AxisToken(LoadOptions.ForwardAxis) + "|U=" + AxisToken(LoadOptions.UpAxis);
	}

	int32 GetAxisBaseIndex(EObjImportAxis Axis)
	{
		switch (Axis)
		{
		case EObjImportAxis::PosX:
		case EObjImportAxis::NegX:
			return 0;
		case EObjImportAxis::PosY:
		case EObjImportAxis::NegY:
			return 1;
		case EObjImportAxis::PosZ:
		case EObjImportAxis::NegZ:
			return 2;
		default:
			return 0;
		}
	}

	float GetAxisSign(EObjImportAxis Axis)
	{
		switch (Axis)
		{
		case EObjImportAxis::NegX:
		case EObjImportAxis::NegY:
		case EObjImportAxis::NegZ:
			return -1.0f;
		default:
			return 1.0f;
		}
	}

	EObjImportAxis GetPositiveAxisByBaseIndex(int32 BaseIndex)
	{
		switch (BaseIndex)
		{
		case 0: return EObjImportAxis::PosX;
		case 1: return EObjImportAxis::PosY;
		case 2: return EObjImportAxis::PosZ;
		default: return EObjImportAxis::PosX;
		}
	}

	EObjImportAxis GetRemainingPositiveAxis(EObjImportAxis ForwardAxis, EObjImportAxis UpAxis)
	{
		const int32 ForwardBaseIndex = GetAxisBaseIndex(ForwardAxis);
		const int32 UpBaseIndex = GetAxisBaseIndex(UpAxis);
		for (int32 BaseIndex = 0; BaseIndex < 3; ++BaseIndex)
		{
			if (BaseIndex != ForwardBaseIndex && BaseIndex != UpBaseIndex)
			{
				return GetPositiveAxisByBaseIndex(BaseIndex);
			}
		}

		return EObjImportAxis::PosY;
	}

	float GetVectorComponentForAxis(const FVector& Vector, EObjImportAxis Axis)
	{
		switch (Axis)
		{
		case EObjImportAxis::PosX: return Vector.X;
		case EObjImportAxis::NegX: return -Vector.X;
		case EObjImportAxis::PosY: return Vector.Y;
		case EObjImportAxis::NegY: return -Vector.Y;
		case EObjImportAxis::PosZ: return Vector.Z;
		case EObjImportAxis::NegZ: return -Vector.Z;
		default: return Vector.X;
		}
	}

	FVector ConvertObjVectorToEngineBasis(const FVector& Vector, const FObjLoadOptions& LoadOptions)
	{
		if (LoadOptions.bUseLegacyObjConversion)
		{
			FVector Converted = Vector;
			Converted.Y = -Converted.Y;
			return Converted;
		}

		const EObjImportAxis RightAxis = GetRemainingPositiveAxis(LoadOptions.ForwardAxis, LoadOptions.UpAxis);
		return FVector(
			GetVectorComponentForAxis(Vector, LoadOptions.ForwardAxis),
			GetVectorComponentForAxis(Vector, RightAxis),
			GetVectorComponentForAxis(Vector, LoadOptions.UpAxis));
	}

	int32 GetObjConversionDeterminantSign(const FObjLoadOptions& LoadOptions)
	{
		if (LoadOptions.bUseLegacyObjConversion)
		{
			return -1;
		}

		const EObjImportAxis RightAxis = GetRemainingPositiveAxis(LoadOptions.ForwardAxis, LoadOptions.UpAxis);
		float Matrix[3][3] = {};
		Matrix[0][GetAxisBaseIndex(LoadOptions.ForwardAxis)] = GetAxisSign(LoadOptions.ForwardAxis);
		Matrix[1][GetAxisBaseIndex(RightAxis)] = GetAxisSign(RightAxis);
		Matrix[2][GetAxisBaseIndex(LoadOptions.UpAxis)] = GetAxisSign(LoadOptions.UpAxis);

		const float Determinant =
			Matrix[0][0] * (Matrix[1][1] * Matrix[2][2] - Matrix[1][2] * Matrix[2][1]) -
			Matrix[0][1] * (Matrix[1][0] * Matrix[2][2] - Matrix[1][2] * Matrix[2][0]) +
			Matrix[0][2] * (Matrix[1][0] * Matrix[2][1] - Matrix[1][1] * Matrix[2][0]);
		return (Determinant < 0.0f) ? -1 : 1;
	}

	FString WideToUtf8(const std::wstring& WideString)
	{
		if (WideString.empty())
		{
			return "";
		}

		const int32 RequiredBytes = ::WideCharToMultiByte(
			CP_UTF8,
			0,
			WideString.c_str(),
			-1,
			nullptr,
			0,
			nullptr,
			nullptr);
		if (RequiredBytes <= 1)
		{
			return "";
		}

		FString Utf8String;
		Utf8String.resize(static_cast<size_t>(RequiredBytes));
		::WideCharToMultiByte(
			CP_UTF8,
			0,
			WideString.c_str(),
			-1,
			Utf8String.data(),
			RequiredBytes,
			nullptr,
			nullptr);
		Utf8String.pop_back();
		return Utf8String;
	}

	FString PathToUtf8(const std::filesystem::path& Path)
	{
		return WideToUtf8(Path.wstring());
	}

	FString TrimAscii(const FString& Value)
	{
		size_t Start = 0;
		while (Start < Value.size() && std::isspace(static_cast<unsigned char>(Value[Start])))
		{
			++Start;
		}

		size_t End = Value.size();
		while (End > Start && std::isspace(static_cast<unsigned char>(Value[End - 1])))
		{
			--End;
		}

		return Value.substr(Start, End - Start);
	}

	bool PathExists(const std::filesystem::path& Path)
	{
		std::error_code ErrorCode;
		return !Path.empty() && std::filesystem::exists(Path, ErrorCode);
	}

	template <typename T>
	bool WriteBinaryValue(std::ofstream& File, const T& Value)
	{
		File.write(reinterpret_cast<const char*>(&Value), sizeof(T));
		return File.good();
	}

	template <typename T>
	bool ReadBinaryValue(std::ifstream& File, T& Value)
	{
		File.read(reinterpret_cast<char*>(&Value), sizeof(T));
		return File.good();
	}

	bool WriteBinaryBytes(std::ofstream& File, const void* Data, std::streamsize Size)
	{
		if (Size <= 0)
		{
			return true;
		}

		File.write(reinterpret_cast<const char*>(Data), Size);
		return File.good();
	}

	bool ReadBinaryBytes(std::ifstream& File, void* Data, std::streamsize Size)
	{
		if (Size <= 0)
		{
			return true;
		}

		File.read(reinterpret_cast<char*>(Data), Size);
		return File.good();
	}

	bool WriteUtf8String(std::ofstream& File, const FString& Value)
	{
		const uint32 ByteCount = static_cast<uint32>(Value.size());
		if (!WriteBinaryValue(File, ByteCount))
		{
			return false;
		}

		return WriteBinaryBytes(File, Value.data(), static_cast<std::streamsize>(ByteCount));
	}

	bool ReadUtf8String(std::ifstream& File, FString& OutValue)
	{
		uint32 ByteCount = 0;
		if (!ReadBinaryValue(File, ByteCount))
		{
			return false;
		}

		OutValue.resize(ByteCount);
		return ReadBinaryBytes(File, OutValue.data(), static_cast<std::streamsize>(ByteCount));
	}

	std::filesystem::path ResolveMaterialReferencePath(const std::filesystem::path& ObjPath, const FString& MaterialReference)
	{
		const std::filesystem::path ReferencePath = std::filesystem::path(FPaths::ToWide(MaterialReference)).lexically_normal();
		if (ReferencePath.is_absolute() && PathExists(ReferencePath))
		{
			return ReferencePath;
		}

		const TArray<std::filesystem::path> Candidates =
		{
			(ObjPath.parent_path() / ReferencePath).lexically_normal(),
			(FPaths::MaterialDir() / ReferencePath).lexically_normal(),
			(FPaths::MaterialDir() / ReferencePath.filename()).lexically_normal(),
			(FPaths::ProjectRoot() / ReferencePath).lexically_normal()
		};

		for (const std::filesystem::path& Candidate : Candidates)
		{
			if (PathExists(Candidate))
			{
				return Candidate;
			}
		}

		return (ObjPath.parent_path() / ReferencePath).lexically_normal();
	}

	std::filesystem::path ResolveTextureReferencePath(const std::filesystem::path& SourceFilePath, const FString& TextureReference)
	{
		const FString TrimmedReference = TrimAscii(TextureReference);
		if (TrimmedReference.empty())
		{
			return {};
		}

		const std::filesystem::path ReferencePath = std::filesystem::path(FPaths::ToWide(TrimmedReference)).lexically_normal();
		if (ReferencePath.is_absolute() && PathExists(ReferencePath))
		{
			return ReferencePath;
		}

		const TArray<std::filesystem::path> Candidates =
		{
			(SourceFilePath.parent_path() / ReferencePath).lexically_normal(),
			(FPaths::ProjectRoot() / ReferencePath).lexically_normal(),
			(FPaths::TextureDir() / ReferencePath).lexically_normal(),
			(FPaths::TextureDir() / ReferencePath.filename()).lexically_normal()
		};

		for (const std::filesystem::path& Candidate : Candidates)
		{
			if (PathExists(Candidate))
			{
				return Candidate;
			}
		}

		return (SourceFilePath.parent_path() / ReferencePath).lexically_normal();
	}

	FString MakeStoredTexturePath(const std::filesystem::path& ModelFilePath, const std::filesystem::path& TexturePath)
	{
		if (TexturePath.empty())
		{
			return "";
		}

		const std::filesystem::path BaseDirectory = ModelFilePath.parent_path().empty()
			? FPaths::ProjectRoot()
			: ModelFilePath.parent_path();
		const std::filesystem::path RelativePath = TexturePath.lexically_relative(BaseDirectory);
		if (!RelativePath.empty())
		{
			return PathToUtf8(RelativePath);
		}

		return PathToUtf8(TexturePath);
	}

	FString GetNormalizedExtension(const FString& PathFileName)
	{
		FString Extension = FPaths::FromPath(FPaths::ToPath(PathFileName).extension());
		std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char Ch)
		{
			return static_cast<char>(std::tolower(Ch));
		});
		return Extension;
	}

	uint32 GetRequiredMaterialSlotCount(const FStaticMesh& StaticMesh, const TArray<FString>& MaterialSlotNames)
	{
		uint32 SlotCount = static_cast<uint32>(MaterialSlotNames.size());
		for (const FMeshSection& Section : StaticMesh.Sections)
		{
			SlotCount = (std::max)(SlotCount, Section.MaterialIndex + 1);
		}
		return SlotCount;
	}

	uint32 GetRequiredMaterialSlotCount(const FStaticMesh& StaticMesh, const TArray<FModelMaterialInfo>& MaterialInfos)
	{
		uint32 SlotCount = static_cast<uint32>(MaterialInfos.size());
		for (const FMeshSection& Section : StaticMesh.Sections)
		{
			SlotCount = (std::max)(SlotCount, Section.MaterialIndex + 1);
		}
		return SlotCount;
	}

	FString GetMaterialSlotNameOrDefault(const TArray<FString>& MaterialSlotNames, uint32 SlotIndex)
	{
		if (SlotIndex < MaterialSlotNames.size() && !MaterialSlotNames[SlotIndex].empty())
		{
			return MaterialSlotNames[SlotIndex];
		}

		return "M_Default";
	}

	FModelMaterialInfo GetMaterialInfoOrDefault(const TArray<FModelMaterialInfo>& MaterialInfos, uint32 SlotIndex)
	{
		if (SlotIndex < MaterialInfos.size())
		{
			FModelMaterialInfo MaterialInfo = MaterialInfos[SlotIndex];
			if (MaterialInfo.Name.empty())
			{
				MaterialInfo.Name = "M_Default";
			}
			return MaterialInfo;
		}

		return {};
	}

	std::shared_ptr<FMaterial> CreateImportedMaterialTemplate(const FString& MaterialName)
	{
		std::shared_ptr<FMaterial> Material = std::make_shared<FMaterial>();
		Material->SetOriginName(MaterialName.empty() ? "M_Default" : MaterialName);

		std::wstring VSPath = FPaths::ShaderDir() / L"VertexShader.hlsl";
		std::wstring PSPath = FPaths::ShaderDir() / L"ColorPixelShader.hlsl";
		Material->SetVertexShader(FShaderMap::Get().GetOrCreateVertexShader(GEngine->GetRenderer()->GetDevice(), VSPath.c_str()));
		Material->SetPixelShader(FShaderMap::Get().GetOrCreatePixelShader(GEngine->GetRenderer()->GetDevice(), PSPath.c_str()));

		int32 SlotIndex = Material->CreateConstantBuffer(GEngine->GetRenderer()->GetDevice(), 32);
		if (SlotIndex >= 0)
		{
			Material->RegisterParameter("BaseColor", SlotIndex, 0, 16);
			const float White[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			Material->GetConstantBuffer(SlotIndex)->SetData(White, sizeof(White));

			Material->RegisterParameter("UVScrollSpeed", SlotIndex, 16, 16);
			const float DefaultScroll[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			Material->GetConstantBuffer(SlotIndex)->SetData(DefaultScroll, sizeof(DefaultScroll), 16);
		}

		return Material;
	}

	void ApplyBaseColorToMaterial(const std::shared_ptr<FMaterial>& Material, const FVector4& BaseColor)
	{
		if (!Material)
		{
			return;
		}

		const float DiffuseColor[4] = { BaseColor.X, BaseColor.Y, BaseColor.Z, BaseColor.W };
		if (FMaterialConstantBuffer* ConstantBuffer = Material->GetConstantBuffer(0))
		{
			ConstantBuffer->SetData(DiffuseColor, sizeof(DiffuseColor));
		}
	}

	bool TryLoadTextureIntoMaterial(const std::shared_ptr<FMaterial>& Material, const std::filesystem::path& TexturePath, const char* LogPrefix)
	{
		if (!Material || TexturePath.empty())
		{
			return false;
		}

		if (!GEngine || !GEngine->GetRenderer() || !GEngine->GetRenderer()->GetDevice())
		{
			return false;
		}

		FRenderer* Renderer = GEngine->GetRenderer();
		ID3D11Device* Device = Renderer->GetDevice();

		ID3D11ShaderResourceView* NewSRV = nullptr;
		if (!Renderer->CreateTextureFromSTB(Device, TexturePath, &NewSRV))
		{
			return false;
		}

		D3D11_SAMPLER_DESC SamplerDesc = {};
		SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		SamplerDesc.MinLOD = 0;
		SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		ID3D11SamplerState* NewSampler = nullptr;
		if (FAILED(Device->CreateSamplerState(&SamplerDesc, &NewSampler)))
		{
			NewSRV->Release();
			return false;
		}

		auto MaterialTexture = std::make_shared<FMaterialTexture>();
		MaterialTexture->SetResources(NewSRV, NewSampler, true);
		Material->SetMaterialTexture(MaterialTexture);

		std::wstring TexPSPath = FPaths::ShaderDir() / L"TexturePixelShader.hlsl";
		Material->SetPixelShader(FShaderMap::Get().GetOrCreatePixelShader(Device, TexPSPath.c_str()));

		std::wstring TexVSPath = FPaths::ShaderDir() / L"TextureVertexShader.hlsl";
		Material->SetVertexShader(FShaderMap::Get().GetOrCreateVertexShader(Device, TexVSPath.c_str()));
		UE_LOG("%s %s", LogPrefix, WideToUtf8(TexturePath.wstring()).c_str());
		return true;
	}

	UStaticMesh* FinalizeStaticMeshAsset(
		const FString& PathFileName,
		std::unique_ptr<FStaticMesh> RawData,
		const TArray<FString>& MaterialSlotNames)
	{
		FString JustFileName = FPaths::FromPath(FPaths::ToPath(PathFileName).filename());

		RawData->PathFileName = JustFileName;
		RawData->UpdateLocalBound();

		UStaticMesh* NewAsset = FObjectFactory::ConstructObject<UStaticMesh>(nullptr, JustFileName);
		NewAsset->SetStaticMeshAsset(RawData.release());

		NewAsset->LocalBounds.Radius = NewAsset->GetRenderData()->GetLocalBoundRadius();
		NewAsset->LocalBounds.Center = NewAsset->GetRenderData()->GetCenterCoord();
		NewAsset->LocalBounds.BoxExtent = (NewAsset->GetRenderData()->GetMaxCoord() - NewAsset->GetRenderData()->GetMinCoord()) * 0.5f;

		uint32 SlotCount = GetRequiredMaterialSlotCount(*NewAsset->GetRenderData(), MaterialSlotNames);
		if (SlotCount == 0)
		{
			SlotCount = 1;
		}

		for (uint32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
		{
			const FString MaterialName = GetMaterialSlotNameOrDefault(MaterialSlotNames, SlotIndex);
			std::shared_ptr<FMaterial> Material = FMaterialManager::Get().FindByName(MaterialName);
			if (!Material)
			{
				UE_LOG("[Warning] Static mesh requested missing material '%s'. Falling back to M_Default.", MaterialName.c_str());
				Material = FMaterialManager::Get().FindByName("M_Default");
			}

			NewAsset->AddDefaultMaterial(Material);
		}

		BuildOrLoadLODs(*NewAsset, PathFileName);

		NewAsset->BuildAccelerationStructureIfNeeded();
		return NewAsset;
	}

	UStaticMesh* FinalizeStaticMeshAsset(
		const FString& PathFileName,
		std::unique_ptr<FStaticMesh> RawData,
		const TArray<FModelMaterialInfo>& MaterialInfos)
	{
		FString JustFileName = FPaths::FromPath(FPaths::ToPath(PathFileName).filename());

		RawData->PathFileName = JustFileName;
		RawData->UpdateLocalBound();

		UStaticMesh* NewAsset = FObjectFactory::ConstructObject<UStaticMesh>(nullptr, JustFileName);
		NewAsset->SetStaticMeshAsset(RawData.release());

		NewAsset->LocalBounds.Radius = NewAsset->GetRenderData()->GetLocalBoundRadius();
		NewAsset->LocalBounds.Center = NewAsset->GetRenderData()->GetCenterCoord();
		NewAsset->LocalBounds.BoxExtent = (NewAsset->GetRenderData()->GetMaxCoord() - NewAsset->GetRenderData()->GetMinCoord()) * 0.5f;

		uint32 SlotCount = GetRequiredMaterialSlotCount(*NewAsset->GetRenderData(), MaterialInfos);
		if (SlotCount == 0)
		{
			SlotCount = 1;
		}

		const std::filesystem::path ModelPath = FPaths::ToPath(FPaths::ToAbsolutePath(PathFileName)).lexically_normal();
		for (uint32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
		{
			const FModelMaterialInfo MaterialInfo = GetMaterialInfoOrDefault(MaterialInfos, SlotIndex);

			std::shared_ptr<FMaterial> Material = CreateImportedMaterialTemplate(MaterialInfo.Name);
			ApplyBaseColorToMaterial(Material, MaterialInfo.BaseColor);

			if (!MaterialInfo.DiffuseTexturePath.empty())
			{
				const std::filesystem::path TexturePath = ResolveTextureReferencePath(ModelPath, MaterialInfo.DiffuseTexturePath);
				if (!TryLoadTextureIntoMaterial(Material, TexturePath, "[.Model Loader] Auto-loaded texture-backed pixel shader:"))
				{
					UE_LOG("[.Model Loader] Failed to resolve embedded texture '%s' for material '%s'.",
						MaterialInfo.DiffuseTexturePath.c_str(),
						MaterialInfo.Name.c_str());
				}
			}

			if (!Material)
			{
				Material = FMaterialManager::Get().FindByName("M_Default");
			}

			NewAsset->AddDefaultMaterial(Material);
		}

		BuildOrLoadLODs(*NewAsset, PathFileName);

		NewAsset->BuildAccelerationStructureIfNeeded();
		return NewAsset;
	}

	struct FObjParserContext
	{
		FStaticMesh* OutMesh = nullptr;
		TArray<FString>& OutMaterialNames;

		TArray<FVector> TempPositions;
		TArray<FVector2> TempUVs;
		TArray<FVector> TempNormals;
		const FObjLoadOptions& LoadOptions;

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

		FObjParserContext(FStaticMesh* InOutMesh, TArray<FString>& InOutMaterialNames, const FObjLoadOptions& InLoadOptions)
			: OutMesh(InOutMesh)
			, OutMaterialNames(InOutMaterialNames)
			, LoadOptions(InLoadOptions)
		{
		}

		void CloseCurrentSection()
		{
			if (OutMesh->Indices.size() > CurrentSectionStartIndex)
			{
				FMeshSection Section{};
				Section.MaterialIndex = static_cast<uint32>(CurrentMaterialIndex);
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

			CurrentMaterialIndex = static_cast<int32>(OutMaterialNames.size());
			OutMaterialNames.push_back(FString(MaterialName.c_str()));
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
				std::string PositionString;
				std::string UVString;
				std::string NormalString;

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
				if (GetObjConversionDeterminantSign(LoadOptions) < 0)
				{
					OutMesh->Indices.push_back(FaceIndices[0]);
					OutMesh->Indices.push_back(FaceIndices[i + 1]);
					OutMesh->Indices.push_back(FaceIndices[i]);
				}
				else
				{
					OutMesh->Indices.push_back(FaceIndices[0]);
					OutMesh->Indices.push_back(FaceIndices[i]);
					OutMesh->Indices.push_back(FaceIndices[i + 1]);
				}
			}
		}
	};
}

UStaticMesh* FObjManager::LoadStaticMeshAsset(const FString& PathFileName)
{
	FString StandardizedPath = GetStandardizedMeshPath(PathFileName);
	const FString Extension = GetNormalizedExtension(StandardizedPath);
	if (Extension == ".obj" || Extension.empty())
	{
		return LoadObjStaticMeshAsset(StandardizedPath);
	}

	if (Extension == ".model")
	{
		return LoadModelStaticMeshAsset(StandardizedPath);
	}

	UE_LOG("[FObjManager] Unsupported static mesh extension: %s", PathFileName.c_str());
	return nullptr;
}

UStaticMesh* FObjManager::LoadObjStaticMeshAsset(const FString& PathFileName)
{
	return LoadObjStaticMeshAsset(PathFileName, FObjLoadOptions{});
}

UStaticMesh* FObjManager::LoadObjStaticMeshAsset(const FString& PathFileName, const FObjLoadOptions& LoadOptions)
{
	const FString CacheKey = BuildObjCacheKey(PathFileName, LoadOptions);

	auto It = ObjStaticMeshMap.find(CacheKey);
	if (It != ObjStaticMeshMap.end())
	{
		return It->second;
	}

	auto RawData = std::make_unique<FStaticMesh>();
	TArray<FString> FoundMaterials;
	if (!ParseObjFile(PathFileName, RawData.get(), FoundMaterials, LoadOptions))
	{
		return nullptr;
	}

	UStaticMesh* NewAsset = FinalizeStaticMeshAsset(PathFileName, std::move(RawData), FoundMaterials);
	ObjStaticMeshMap[CacheKey] = NewAsset;
	return NewAsset;
}

UStaticMesh* FObjManager::LoadModelStaticMeshAsset(const FString& PathFileName)
{
	FString StandardizedPath = GetStandardizedMeshPath(PathFileName);

	auto It = ObjStaticMeshMap.find(StandardizedPath);
	if (It != ObjStaticMeshMap.end())
	{
		return It->second;
	}

	const FString AbsolutePath = FPaths::ToAbsolutePath(PathFileName);
	const std::filesystem::path FilePath = FPaths::ToPath(AbsolutePath).lexically_normal();

	std::ifstream File(FilePath, std::ios::binary);
	if (!File.is_open())
	{
		UE_LOG("[FObjManager] Failed to open .Model file: %s", AbsolutePath.c_str());
		return nullptr;
	}

	char Magic[sizeof(GModelMagic)] = {};
	if (!ReadBinaryBytes(File, Magic, sizeof(Magic)) || std::memcmp(Magic, GModelMagic, sizeof(GModelMagic)) != 0)
	{
		UE_LOG("[FObjManager] Invalid .Model header: %s", AbsolutePath.c_str());
		return nullptr;
	}

	uint32 Version = 0;
	uint32 VertexCount = 0;
	uint32 IndexCount = 0;
	uint32 SectionCount = 0;
	uint32 MaterialSlotCount = 0;
	if (!ReadBinaryValue(File, Version)
		|| !ReadBinaryValue(File, VertexCount)
		|| !ReadBinaryValue(File, IndexCount)
		|| !ReadBinaryValue(File, SectionCount)
		|| !ReadBinaryValue(File, MaterialSlotCount))
	{
		UE_LOG("[FObjManager] Failed to read .Model header: %s", AbsolutePath.c_str());
		return nullptr;
	}

	if (Version != GModelVersionLegacy && Version != GModelVersionEmbeddedMaterials)
	{
		UE_LOG("[FObjManager] Unsupported .Model version %u: %s", Version, AbsolutePath.c_str());
		return nullptr;
	}

	auto RawData = std::make_unique<FStaticMesh>();
	RawData->Topology = EMeshTopology::EMT_TriangleList;
	RawData->Vertices.resize(VertexCount);
	RawData->Indices.resize(IndexCount);
	RawData->Sections.resize(SectionCount);

	for (FVertex& Vertex : RawData->Vertices)
	{
		if (!ReadBinaryValue(File, Vertex.Position.X)
			|| !ReadBinaryValue(File, Vertex.Position.Y)
			|| !ReadBinaryValue(File, Vertex.Position.Z)
			|| !ReadBinaryValue(File, Vertex.Color.X)
			|| !ReadBinaryValue(File, Vertex.Color.Y)
			|| !ReadBinaryValue(File, Vertex.Color.Z)
			|| !ReadBinaryValue(File, Vertex.Color.W)
			|| !ReadBinaryValue(File, Vertex.Normal.X)
			|| !ReadBinaryValue(File, Vertex.Normal.Y)
			|| !ReadBinaryValue(File, Vertex.Normal.Z)
			|| !ReadBinaryValue(File, Vertex.UV.X)
			|| !ReadBinaryValue(File, Vertex.UV.Y))
		{
			UE_LOG("[FObjManager] Failed to read .Model vertices: %s", AbsolutePath.c_str());
			return nullptr;
		}
	}

	for (uint32& Index : RawData->Indices)
	{
		if (!ReadBinaryValue(File, Index))
		{
			UE_LOG("[FObjManager] Failed to read .Model indices: %s", AbsolutePath.c_str());
			return nullptr;
		}
	}

	for (FMeshSection& Section : RawData->Sections)
	{
		if (!ReadBinaryValue(File, Section.MaterialIndex)
			|| !ReadBinaryValue(File, Section.StartIndex)
			|| !ReadBinaryValue(File, Section.IndexCount))
		{
			UE_LOG("[FObjManager] Failed to read .Model sections: %s", AbsolutePath.c_str());
			return nullptr;
		}

		const uint64 SectionEndIndex = static_cast<uint64>(Section.StartIndex) + static_cast<uint64>(Section.IndexCount);
		if (SectionEndIndex > RawData->Indices.size())
		{
			UE_LOG("[FObjManager] Invalid .Model section range: %s", AbsolutePath.c_str());
			return nullptr;
		}
	}

	TArray<FString> MaterialSlotNames;
	MaterialSlotNames.resize(MaterialSlotCount);
	for (FString& MaterialSlotName : MaterialSlotNames)
	{
		if (!ReadUtf8String(File, MaterialSlotName))
		{
			UE_LOG("[FObjManager] Failed to read .Model material slots: %s", AbsolutePath.c_str());
			return nullptr;
		}
	}

	if (Version == GModelVersionLegacy)
	{
		UStaticMesh* NewAsset = FinalizeStaticMeshAsset(PathFileName, std::move(RawData), MaterialSlotNames);
		ObjStaticMeshMap[StandardizedPath] = NewAsset;
		return NewAsset;
	}

	TArray<FModelMaterialInfo> MaterialInfos;
	MaterialInfos.resize(MaterialSlotCount);
	for (uint32 SlotIndex = 0; SlotIndex < MaterialSlotCount; ++SlotIndex)
	{
		FModelMaterialInfo& MaterialInfo = MaterialInfos[SlotIndex];
		MaterialInfo.Name = GetMaterialSlotNameOrDefault(MaterialSlotNames, SlotIndex);

		if (!ReadBinaryValue(File, MaterialInfo.BaseColor.X)
			|| !ReadBinaryValue(File, MaterialInfo.BaseColor.Y)
			|| !ReadBinaryValue(File, MaterialInfo.BaseColor.Z)
			|| !ReadBinaryValue(File, MaterialInfo.BaseColor.W)
			|| !ReadUtf8String(File, MaterialInfo.DiffuseTexturePath))
		{
			UE_LOG("[FObjManager] Failed to read .Model material metadata: %s", AbsolutePath.c_str());
			return nullptr;
		}
	}

	UStaticMesh* NewAsset = FinalizeStaticMeshAsset(PathFileName, std::move(RawData), MaterialInfos);
	ObjStaticMeshMap[StandardizedPath] = NewAsset;
	return NewAsset;
}

bool FObjManager::SaveModelStaticMeshAsset(const FString& PathFileName, const FStaticMesh& StaticMesh, const TArray<FModelMaterialInfo>& MaterialInfos)
{
	if (StaticMesh.Topology != EMeshTopology::EMT_TriangleList)
	{
		UE_LOG("[FObjManager] Only triangle-list meshes can be exported as .Model: %s", PathFileName.c_str());
		return false;
	}

	const FString AbsolutePath = FPaths::ToAbsolutePath(PathFileName);
	const std::filesystem::path FilePath = FPaths::ToPath(AbsolutePath).lexically_normal();

	std::error_code ErrorCode;
	if (!FilePath.parent_path().empty())
	{
		std::filesystem::create_directories(FilePath.parent_path(), ErrorCode);
	}

	std::ofstream File(FilePath, std::ios::binary | std::ios::trunc);
	if (!File.is_open())
	{
		UE_LOG("[FObjManager] Failed to create .Model file: %s", AbsolutePath.c_str());
		return false;
	}

	uint32 MaterialSlotCount = GetRequiredMaterialSlotCount(StaticMesh, MaterialInfos);
	if (MaterialSlotCount == 0)
	{
		MaterialSlotCount = 1;
	}

	if (!WriteBinaryBytes(File, GModelMagic, sizeof(GModelMagic))
		|| !WriteBinaryValue(File, GModelVersion)
		|| !WriteBinaryValue(File, static_cast<uint32>(StaticMesh.Vertices.size()))
		|| !WriteBinaryValue(File, static_cast<uint32>(StaticMesh.Indices.size()))
		|| !WriteBinaryValue(File, static_cast<uint32>(StaticMesh.Sections.size()))
		|| !WriteBinaryValue(File, MaterialSlotCount))
	{
		return false;
	}

	for (const FVertex& Vertex : StaticMesh.Vertices)
	{
		if (!WriteBinaryValue(File, Vertex.Position.X)
			|| !WriteBinaryValue(File, Vertex.Position.Y)
			|| !WriteBinaryValue(File, Vertex.Position.Z)
			|| !WriteBinaryValue(File, Vertex.Color.X)
			|| !WriteBinaryValue(File, Vertex.Color.Y)
			|| !WriteBinaryValue(File, Vertex.Color.Z)
			|| !WriteBinaryValue(File, Vertex.Color.W)
			|| !WriteBinaryValue(File, Vertex.Normal.X)
			|| !WriteBinaryValue(File, Vertex.Normal.Y)
			|| !WriteBinaryValue(File, Vertex.Normal.Z)
			|| !WriteBinaryValue(File, Vertex.UV.X)
			|| !WriteBinaryValue(File, Vertex.UV.Y))
		{
			return false;
		}
	}

	for (uint32 Index : StaticMesh.Indices)
	{
		if (!WriteBinaryValue(File, Index))
		{
			return false;
		}
	}

	for (const FMeshSection& Section : StaticMesh.Sections)
	{
		if (!WriteBinaryValue(File, Section.MaterialIndex)
			|| !WriteBinaryValue(File, Section.StartIndex)
			|| !WriteBinaryValue(File, Section.IndexCount))
		{
			return false;
		}
	}

	for (uint32 SlotIndex = 0; SlotIndex < MaterialSlotCount; ++SlotIndex)
	{
		const FModelMaterialInfo MaterialInfo = GetMaterialInfoOrDefault(MaterialInfos, SlotIndex);
		if (!WriteUtf8String(File, MaterialInfo.Name))
		{
			return false;
		}
	}

	for (uint32 SlotIndex = 0; SlotIndex < MaterialSlotCount; ++SlotIndex)
	{
		const FModelMaterialInfo MaterialInfo = GetMaterialInfoOrDefault(MaterialInfos, SlotIndex);
		if (!WriteBinaryValue(File, MaterialInfo.BaseColor.X)
			|| !WriteBinaryValue(File, MaterialInfo.BaseColor.Y)
			|| !WriteBinaryValue(File, MaterialInfo.BaseColor.Z)
			|| !WriteBinaryValue(File, MaterialInfo.BaseColor.W)
			|| !WriteUtf8String(File, MaterialInfo.DiffuseTexturePath))
		{
			return false;
		}
	}

	return File.good();
}

bool FObjManager::SaveLODAsset(const FString& PathFileName, const FStaticMesh& LodMesh)
{
	const FString AbsolutePath = FPaths::ToAbsolutePath(PathFileName);
	const std::filesystem::path FilePath = FPaths::ToPath(AbsolutePath).lexically_normal();

	std::error_code ErrorCode;
	if (!FilePath.parent_path().empty())
	{
		std::filesystem::create_directories(FilePath.parent_path(), ErrorCode);
	}

	std::ofstream File(FilePath, std::ios::binary | std::ios::trunc);
	if (!File.is_open())
	{
		UE_LOG("[FObjManager] Failed to create .lod file: %s", AbsolutePath.c_str());
		return false;
	}

	if (!WriteBinaryBytes(File, GLODMagic, sizeof(GLODMagic))
		|| !WriteBinaryValue(File, GLODVersion)
		|| !WriteBinaryValue(File, static_cast<uint32>(LodMesh.Vertices.size()))
		|| !WriteBinaryValue(File, static_cast<uint32>(LodMesh.Indices.size()))
		|| !WriteBinaryValue(File, static_cast<uint32>(LodMesh.Sections.size())))
	{
		return false;
	}

	for (const FVertex& Vertex : LodMesh.Vertices)
	{
		if (!WriteBinaryValue(File, Vertex.Position.X)
			|| !WriteBinaryValue(File, Vertex.Position.Y)
			|| !WriteBinaryValue(File, Vertex.Position.Z)
			|| !WriteBinaryValue(File, Vertex.Color.X)
			|| !WriteBinaryValue(File, Vertex.Color.Y)
			|| !WriteBinaryValue(File, Vertex.Color.Z)
			|| !WriteBinaryValue(File, Vertex.Color.W)
			|| !WriteBinaryValue(File, Vertex.Normal.X)
			|| !WriteBinaryValue(File, Vertex.Normal.Y)
			|| !WriteBinaryValue(File, Vertex.Normal.Z)
			|| !WriteBinaryValue(File, Vertex.UV.X)
			|| !WriteBinaryValue(File, Vertex.UV.Y))
		{
			return false;
		}
	}

	for (uint32 Index : LodMesh.Indices)
	{
		if (!WriteBinaryValue(File, Index))
		{
			return false;
		}
	}

	for (const FMeshSection& Section : LodMesh.Sections)
	{
		if (!WriteBinaryValue(File, Section.MaterialIndex)
			|| !WriteBinaryValue(File, Section.StartIndex)
			|| !WriteBinaryValue(File, Section.IndexCount))
		{
			return false;
		}
	}

	return File.good();
}

FStaticMesh* FObjManager::LoadLODAsset(const FString& PathFileName)
{
	const FString AbsolutePath = FPaths::ToAbsolutePath(PathFileName);
	const std::filesystem::path FilePath = FPaths::ToPath(AbsolutePath).lexically_normal();

	std::ifstream File(FilePath, std::ios::binary);
	if (!File.is_open())
	{
		return nullptr;
	}

	char Magic[sizeof(GLODMagic)] = {};
	if (!ReadBinaryBytes(File, Magic, sizeof(Magic)) || std::memcmp(Magic, GLODMagic, sizeof(GLODMagic)) != 0)
	{
		UE_LOG("[FObjManager] Invalid .lod header: %s", AbsolutePath.c_str());
		return nullptr;
	}

	uint32 Version = 0;
	uint32 VertexCount = 0;
	uint32 IndexCount = 0;
	uint32 SectionCount = 0;
	if (!ReadBinaryValue(File, Version)
		|| !ReadBinaryValue(File, VertexCount)
		|| !ReadBinaryValue(File, IndexCount)
		|| !ReadBinaryValue(File, SectionCount))
	{
		UE_LOG("[FObjManager] Failed to read .lod header: %s", AbsolutePath.c_str());
		return nullptr;
	}

	if (Version != GLODVersion)
	{
		UE_LOG("[FObjManager] Unsupported .lod version %u: %s", Version, AbsolutePath.c_str());
		return nullptr;
	}

	auto Mesh = std::make_unique<FStaticMesh>();
	Mesh->Topology = EMeshTopology::EMT_TriangleList;
	Mesh->Vertices.resize(VertexCount);
	Mesh->Indices.resize(IndexCount);
	Mesh->Sections.resize(SectionCount);

	for (FVertex& Vertex : Mesh->Vertices)
	{
		if (!ReadBinaryValue(File, Vertex.Position.X)
			|| !ReadBinaryValue(File, Vertex.Position.Y)
			|| !ReadBinaryValue(File, Vertex.Position.Z)
			|| !ReadBinaryValue(File, Vertex.Color.X)
			|| !ReadBinaryValue(File, Vertex.Color.Y)
			|| !ReadBinaryValue(File, Vertex.Color.Z)
			|| !ReadBinaryValue(File, Vertex.Color.W)
			|| !ReadBinaryValue(File, Vertex.Normal.X)
			|| !ReadBinaryValue(File, Vertex.Normal.Y)
			|| !ReadBinaryValue(File, Vertex.Normal.Z)
			|| !ReadBinaryValue(File, Vertex.UV.X)
			|| !ReadBinaryValue(File, Vertex.UV.Y))
		{
			UE_LOG("[FObjManager] Failed to read .lod vertices: %s", AbsolutePath.c_str());
			return nullptr;
		}
	}

	for (uint32& Index : Mesh->Indices)
	{
		if (!ReadBinaryValue(File, Index))
		{
			UE_LOG("[FObjManager] Failed to read .lod indices: %s", AbsolutePath.c_str());
			return nullptr;
		}
	}

	for (FMeshSection& Section : Mesh->Sections)
	{
		if (!ReadBinaryValue(File, Section.MaterialIndex)
			|| !ReadBinaryValue(File, Section.StartIndex)
			|| !ReadBinaryValue(File, Section.IndexCount))
		{
			UE_LOG("[FObjManager] Failed to read .lod sections: %s", AbsolutePath.c_str());
			return nullptr;
		}

		const uint64 SectionEndIndex = static_cast<uint64>(Section.StartIndex) + static_cast<uint64>(Section.IndexCount);
		if (SectionEndIndex > Mesh->Indices.size())
		{
			UE_LOG("[FObjManager] Invalid .lod section range: %s", AbsolutePath.c_str());
			return nullptr;
		}
	}

	Mesh->UpdateLocalBound();
	Mesh->bIsDirty = true;
	return Mesh.release();
}

bool FObjManager::BuildModelMaterialInfosFromObj(
	const FString& ObjFilePath,
	const FString& ModelFilePath,
	const TArray<FString>& MaterialSlotNames,
	TArray<FModelMaterialInfo>& OutMaterialInfos)
{
	const uint32 SlotCount = (std::max)(1u, static_cast<uint32>(MaterialSlotNames.size()));
	OutMaterialInfos.clear();
	OutMaterialInfos.resize(SlotCount);
	for (uint32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		OutMaterialInfos[SlotIndex].Name = GetMaterialSlotNameOrDefault(MaterialSlotNames, SlotIndex);
	}

	const FString AbsoluteObjPath = FPaths::ToAbsolutePath(ObjFilePath);
	const FString AbsoluteModelPath = FPaths::ToAbsolutePath(ModelFilePath);
	const std::filesystem::path ObjPath = FPaths::ToPath(AbsoluteObjPath).lexically_normal();
	const std::filesystem::path ModelPath = FPaths::ToPath(AbsoluteModelPath).lexically_normal();

	std::ifstream ObjFile(ObjPath);
	if (!ObjFile.is_open())
	{
		UE_LOG("[FObjManager] Failed to open OBJ while collecting .Model material data: %s", AbsoluteObjPath.c_str());
		return false;
	}

	struct FParsedMaterialData
	{
		FVector4 BaseColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		FString DiffuseTexturePath;
	};

	TMap<FString, FParsedMaterialData> ParsedMaterials;
	FString ObjLine;
	while (std::getline(ObjFile, ObjLine))
	{
		if (ObjLine.empty() || ObjLine[0] == '#')
		{
			continue;
		}

		std::stringstream ObjSS(ObjLine);
		FString Type;
		ObjSS >> Type;
		if (Type != "mtllib")
		{
			continue;
		}

		FString MaterialReference;
		std::getline(ObjSS, MaterialReference);
		MaterialReference = TrimAscii(MaterialReference);
		if (MaterialReference.empty())
		{
			continue;
		}

		const std::filesystem::path MtlPath = ResolveMaterialReferencePath(ObjPath, MaterialReference);
		std::ifstream MtlFile(MtlPath);
		if (!MtlFile.is_open())
		{
			const FString MtlPathUtf8 = FPaths::FromPath(MtlPath);
			UE_LOG("[FObjManager] Failed to open MTL while collecting .Model material data: %s", MtlPathUtf8.c_str());
			continue;
		}

		FString CurrentMaterialName;
		FString MtlLine;
		while (std::getline(MtlFile, MtlLine))
		{
			if (MtlLine.empty() || MtlLine[0] == '#')
			{
				continue;
			}

			std::stringstream MtlSS(MtlLine);
			FString MtlType;
			MtlSS >> MtlType;

			if (MtlType == "newmtl")
			{
				MtlSS >> CurrentMaterialName;
				if (!CurrentMaterialName.empty())
				{
					ParsedMaterials.try_emplace(CurrentMaterialName, FParsedMaterialData{});
				}
			}
			else if (MtlType == "Kd" && !CurrentMaterialName.empty())
			{
				float R = 1.0f;
				float G = 1.0f;
				float B = 1.0f;
				MtlSS >> R >> G >> B;
				ParsedMaterials[CurrentMaterialName].BaseColor = FVector4(R, G, B, 1.0f);
			}
			else if (MtlType == "map_Kd" && !CurrentMaterialName.empty())
			{
				FString TextureReference;
				std::getline(MtlSS, TextureReference);
				TextureReference = TrimAscii(TextureReference);
				if (TextureReference.empty())
				{
					continue;
				}

				const std::filesystem::path TexturePath = ResolveTextureReferencePath(MtlPath, TextureReference);
				if (PathExists(TexturePath))
				{
					ParsedMaterials[CurrentMaterialName].DiffuseTexturePath = MakeStoredTexturePath(ModelPath, TexturePath);
				}
				else
				{
					UE_LOG("[FObjManager] Failed to resolve MTL texture '%s' for material '%s'.",
						TextureReference.c_str(),
						CurrentMaterialName.c_str());
				}
			}
		}
	}

	for (FModelMaterialInfo& MaterialInfo : OutMaterialInfos)
	{
		auto It = ParsedMaterials.find(MaterialInfo.Name);
		if (It != ParsedMaterials.end())
		{
			MaterialInfo.BaseColor = It->second.BaseColor;
			MaterialInfo.DiffuseTexturePath = It->second.DiffuseTexturePath;
		}
	}

	return true;
}

bool FObjManager::ParseMtlFile(const FString& MtlFIlePath)
{
	const FString AbsolutePath = FPaths::ToAbsolutePath(MtlFIlePath);
	const std::filesystem::path FilePath = FPaths::ToPath(AbsolutePath).lexically_normal();

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

			CurrentMaterial = CreateImportedMaterialTemplate(MaterialName.c_str());
			FMaterialManager::Get().Register(MaterialName.c_str(), CurrentMaterial);
		}
		else if (Type == "Kd" && CurrentMaterial)
		{
			float R = 0.0f;
			float G = 0.0f;
			float B = 0.0f;
			SS >> R >> G >> B;

			ApplyBaseColorToMaterial(CurrentMaterial, FVector4(R, G, B, 1.0f));
		}
		else if (Type == "map_Kd" && CurrentMaterial)
		{
			FString TextureReference;
			std::getline(SS, TextureReference);
			TextureReference = TrimAscii(TextureReference);

			const std::filesystem::path TexturePath = ResolveTextureReferencePath(FilePath, TextureReference);
			if (!TryLoadTextureIntoMaterial(CurrentMaterial, TexturePath, "[MTL Parser] Auto-loaded texture-backed pixel shader:"))
			{
				UE_LOG("[MTL Parser] Failed to resolve texture '%s' referenced by '%s'.",
					TextureReference.c_str(),
					AbsolutePath.c_str());
			}
		}
	}

	return true;
}

void FObjManager::PreloadAllObjFiles(const FString& DirectoryPath)
{
	const FString AbsolutePath = FPaths::ToAbsolutePath(DirectoryPath);
	const std::filesystem::path DirPath = FPaths::ToPath(AbsolutePath).lexically_normal();

	// 전달된 경로가 실제 디렉터리인지 확인한다.
	if (!std::filesystem::exists(DirPath) || !std::filesystem::is_directory(DirPath))
	{
		UE_LOG("[FObjManager] Preload 실패: 디렉터리를 찾을 수 없습니다. (%s)", AbsolutePath.c_str());
		return;
	}

	for (const auto& Entry : std::filesystem::directory_iterator(DirPath))
	{
		if (Entry.is_regular_file() && Entry.path().extension() == ".obj")
		{
			FString FullFilePath = FPaths::FromPath(Entry.path());

			UStaticMesh* LoadedMesh = LoadObjStaticMeshAsset(FullFilePath.c_str());
		}
	}
}

void FObjManager::PreloadAllModelFiles(const FString& DirectoryPath)
{
	const FString AbsolutePath = FPaths::ToAbsolutePath(DirectoryPath);
	const std::filesystem::path DirPath = FPaths::ToPath(AbsolutePath).lexically_normal();

	// 전달된 경로가 실제 디렉터리인지 확인한다.
	if (!std::filesystem::exists(DirPath) || !std::filesystem::is_directory(DirPath))
	{
		UE_LOG("[FObjManager] Preload 실패: 디렉터리를 찾을 수 없습니다. (%s)", AbsolutePath.c_str());
		return;
	}

	for (const auto& Entry : std::filesystem::directory_iterator(DirPath))
	{
		if (Entry.is_regular_file() && GetNormalizedExtension(FPaths::FromPath(Entry.path())) == ".model")
		{
			FString FullFilePath = FPaths::FromPath(Entry.path());

			UStaticMesh* LoadedMesh = LoadModelStaticMeshAsset(FullFilePath.c_str());
		}
	}
	PreloadAllMtlFiles(FPaths::FromPath(FPaths::MaterialDir()).c_str());
}

void FObjManager::PreloadAllMtlFiles(const FString& DirectoryPath)
{
	const FString AbsolutePath = FPaths::ToAbsolutePath(DirectoryPath);
	const std::filesystem::path DirPath = FPaths::ToPath(AbsolutePath).lexically_normal();

	if (!std::filesystem::exists(DirPath) || !std::filesystem::is_directory(DirPath))
	{
		UE_LOG("[FObjManager] MTL Preload 실패: 디렉터리를 찾을 수 없습니다. (%s)", AbsolutePath.c_str());
		return;
	}

	for (const auto& Entry : std::filesystem::directory_iterator(DirPath))
	{
		if (Entry.is_regular_file() && GetNormalizedExtension(FPaths::FromPath(Entry.path())) == ".mtl")
		{
			FString FullFilePath = FPaths::FromPath(Entry.path());
			ParseMtlFile(FullFilePath.c_str());
		}
	}
}
bool FObjManager::ParseObjFile(const FString& FilePath, FStaticMesh* OutMesh, TArray<FString>& OutMaterialNames, const FObjLoadOptions& LoadOptions)
{
	const FString AbsolutePath = FPaths::ToAbsolutePath(FilePath);
	const std::filesystem::path ObjPath = FPaths::ToPath(AbsolutePath).lexically_normal();

	if (!LoadOptions.bUseLegacyObjConversion &&
		GetAxisBaseIndex(LoadOptions.ForwardAxis) == GetAxisBaseIndex(LoadOptions.UpAxis))
	{
		UE_LOG("[FObjManager] Invalid OBJ axis conversion pair for file: %s", AbsolutePath.c_str());
		return false;
	}

	std::ifstream File(ObjPath);
	if (!File.is_open())
	{
		UE_LOG("[FObjManager] Failed to open OBJ file: %s", AbsolutePath.c_str());
		return false;
	}

	FObjParserContext Context(OutMesh, OutMaterialNames, LoadOptions);
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
			std::string MtlFileName;
			SS >> MtlFileName;

			const std::filesystem::path ResolvedMtlPath = ResolveMaterialReferencePath(ObjPath, MtlFileName.c_str());
			ParseMtlFile(FPaths::FromPath(ResolvedMtlPath).c_str());
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
			Context.TempPositions.push_back(ConvertObjVectorToEngineBasis(Position, LoadOptions));
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
			Context.TempNormals.push_back(ConvertObjVectorToEngineBasis(Normal, LoadOptions));
		}
	}

	Context.CloseCurrentSection();
	OutMesh->Topology = EMeshTopology::EMT_TriangleList;

	UE_LOG(
		"[FObjManager] Parsed OBJ: %s (Verts: %zu, Indices: %zu)",
		AbsolutePath.c_str(),
		OutMesh->Vertices.size(),
		OutMesh->Indices.size());

	return true;
}

void FObjManager::ClearCache()
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
