#include "MaterialManager.h"

#include "Core/Engine.h"
#include "Core/Paths.h"
#include "Debug/EngineLog.h"
#include "Material.h"
#include "Renderer/Renderer.h"
#include "Shader.h"
#include "ShaderMap.h"
#include "ThirdParty/nlohmann/json.hpp"
// HLSL cbuffer 레이아웃을 계산할 때 사용하는 보조 함수들이다.
#include <fstream>

namespace
{
	// 타입 문자열을 바이트 크기로 변환한다.
	uint32 GetTypeSize(const FString& Type)
	{
		if (Type == "float") return 4;
		if (Type == "float2") return 8;
		if (Type == "float3") return 12;
		if (Type == "float4") return 16;
		if (Type == "float4x4") return 64;
		return 0;
	}
	// HLSL cbuffer 규칙에 맞춰 16바이트 경계를 넘지 않도록 오프셋을 정렬한다.
	uint32 AlignOffset(uint32 Offset, uint32 TypeSize)
	{
		// 현재 16바이트 블록에서 남은 공간을 계산한다.
		const uint32 BoundaryStart = (Offset / 16) * 16;
		const uint32 Remaining = BoundaryStart + 16 - Offset;
		// 현재 블록에 담을 수 없으면 다음 16바이트 경계로 넘긴다.
		if (TypeSize > Remaining)
		{
			return BoundaryStart + 16;
		}
		return Offset;
	}

	// 상수 버퍼 전체 크기도 16바이트 배수로 맞춘다.
	uint32 AlignBufferSize(uint32 Size)
	{
		return (Size + 15) & ~15;
	}
}

// FMaterialManager 구현

FMaterialManager& FMaterialManager::Get()
{
	static FMaterialManager Instance;
	return Instance;
}

void FMaterialManager::LoadAllMaterials(ID3D11Device* InDevice, FRenderStateManager* InStateManager)
{
	// Material 디렉터리를 순회하며 JSON 머티리얼 파일을 미리 로드한다.
	namespace fs = std::filesystem;
	const auto MaterialDir = FPaths::MaterialDir();

	try
	{
		if (!fs::exists(MaterialDir))
		{
			UE_LOG("[MaterialManager] Material dir not exists\n");
			return;
		}

		// 모든 JSON 파일을 찾아 순차적으로 적재한다.
		for (const auto& Entry : fs::directory_iterator(MaterialDir))
		{
			if (Entry.is_regular_file() && Entry.path().extension() == ".json")
			{
				const FString FilePath = FPaths::FromPath(Entry.path());
				LoadFromFile(InDevice, InStateManager, FilePath);
			}
		}
	}
	catch (const fs::filesystem_error& Ex)
	{
		UE_LOG("[MaterialManager] Filesystem Error while preload materials: %s\n", Ex.what());
	}
}

std::shared_ptr<FMaterial> FMaterialManager::LoadFromFile(
	ID3D11Device* InDevice,
	FRenderStateManager* InStateManager,
	const FString& InFilePath)
{
	(void)InStateManager;

	// 동일한 경로를 다시 요청하면 캐시를 재사용한다.
	auto PathIt = PathCache.find(InFilePath);
	if (PathIt != PathCache.end())
	{
		return PathIt->second;
	}
	// JSON 파일을 열고 파싱한다.
	std::ifstream File(FPaths::ToPath(InFilePath));
	if (!File.is_open())
	{
		return nullptr;
	}

	nlohmann::json Json;
	try
	{
		File >> Json;
	}
	catch (...)
	{
		return nullptr;
	}
	// Material 객체를 생성한다.
	auto Material = std::make_shared<FMaterial>();
	// 셰이더 경로를 읽어 필요한 버텍스/픽셀 셰이더를 연결한다.
	if (Json.contains("VertexShader"))
	{
		const FString VSRelPath = Json["VertexShader"].get<FString>();
		const std::wstring VSPath = (FPaths::ProjectRoot() / FPaths::ToPath(VSRelPath)).wstring();
		Material->SetVertexShader(FShaderMap::Get().GetOrCreateVertexShader(InDevice, VSPath.c_str()));
	}

	if (Json.contains("PixelShader"))
	{
		const FString PSRelPath = Json["PixelShader"].get<FString>();
		const std::wstring PSPath = (FPaths::ProjectRoot() / FPaths::ToPath(PSRelPath)).wstring();
		Material->SetPixelShader(FShaderMap::Get().GetOrCreatePixelShader(InDevice, PSPath.c_str()));
	}

	if (Json.contains("Textures"))
	{
		// "Diffuse" 슬롯에 지정된 텍스처를 머티리얼 텍스처로 로드한다.
		auto TexturesJson = Json["Textures"];
		if (TexturesJson.contains("Diffuse"))
		{
			const FString TextureRelPath = TexturesJson["Diffuse"].get<FString>();
			const std::filesystem::path FullTexturePath = FPaths::AssetDir() / FPaths::ToPath(TextureRelPath);

			ID3D11ShaderResourceView* NewSRV = nullptr;
			if (GEngine->GetRenderer()->CreateTextureFromSTB(InDevice, FullTexturePath, &NewSRV))
			{
				// FMaterialTexture 래퍼를 만들어 머티리얼에 연결한다.
				auto MaterialTexture = std::make_shared<FMaterialTexture>();
				MaterialTexture->TextureSRV = NewSRV;
				Material->SetMaterialTexture(MaterialTexture);
			}
		}
	}

	// 상수 버퍼 정의를 읽어 머티리얼 파라미터를 구성한다.
	if (Json.contains("ConstantBuffers"))
	{
		for (auto& ConstantBufferJson : Json["ConstantBuffers"])
		{
			if (!ConstantBufferJson.contains("Parameters"))
			{
				continue;
			}

			// 1단계: 파라미터 타입과 정렬 규칙을 적용해 레이아웃을 계산한다.
			struct FParamInfo
			{
				FString Name;
				uint32 Offset = 0;
				uint32 Size = 0;
				FString Type;
				nlohmann::json Value;
			};

			TArray<FParamInfo> ParameterList;
			uint32 CurrentOffset = 0;

			for (auto& ParameterJson : ConstantBufferJson["Parameters"])
			{
				const FString Type = ParameterJson.value("Type", "");
				const uint32 TypeSize = GetTypeSize(Type);
				if (TypeSize == 0)
				{
					continue;
				}

				const uint32 AlignedOffset = AlignOffset(CurrentOffset, TypeSize);

				FParamInfo Info;
				Info.Name = ParameterJson.value("Name", "");
				Info.Offset = AlignedOffset;
				Info.Size = TypeSize;
				Info.Type = Type;
				Info.Value = ParameterJson.contains("Value") ? ParameterJson["Value"] : nlohmann::json();
				ParameterList.push_back(Info);

				CurrentOffset = AlignedOffset + TypeSize;
			}

			if (CurrentOffset == 0)
			{
				continue;
			}

			// 2단계: 계산된 크기로 상수 버퍼를 생성한다.
			const uint32 BufferSize = AlignBufferSize(CurrentOffset);
			const int32 SlotIndex = Material->CreateConstantBuffer(InDevice, BufferSize);
			if (SlotIndex < 0)
			{
				continue;
			}

			FMaterialConstantBuffer* ConstantBuffer = Material->GetConstantBuffer(SlotIndex);
			if (!ConstantBuffer)
			{
				continue;
			}

			// 이름이 있는 파라미터는 런타임 조회를 위해 등록한다.
			for (const FParamInfo& Info : ParameterList)
			{
				if (!Info.Name.empty())
				{
					Material->RegisterParameter(Info.Name, SlotIndex, Info.Offset, Info.Size);
				}
			}

			// 3단계: 초기값이 있는 파라미터는 상수 버퍼에 바로 기록한다.
			for (const FParamInfo& Info : ParameterList)
			{
				if (Info.Value.is_null())
				{
					continue;
				}

				if (Info.Type == "float" && Info.Value.is_number())
				{
					const float Value = Info.Value.get<float>();
					ConstantBuffer->SetData(&Value, sizeof(float), Info.Offset);
				}
				else if (Info.Type == "float2" && Info.Value.is_array() && Info.Value.size() >= 2)
				{
					const float Value[2] =
					{
						Info.Value[0].get<float>(),
						Info.Value[1].get<float>()
					};
					ConstantBuffer->SetData(Value, sizeof(Value), Info.Offset);
				}
				else if (Info.Type == "float3" && Info.Value.is_array() && Info.Value.size() >= 3)
				{
					const float Value[3] =
					{
						Info.Value[0].get<float>(),
						Info.Value[1].get<float>(),
						Info.Value[2].get<float>()
					};
					ConstantBuffer->SetData(Value, sizeof(Value), Info.Offset);
				}
				else if (Info.Type == "float4" && Info.Value.is_array() && Info.Value.size() >= 4)
				{
					const float Value[4] =
					{
						Info.Value[0].get<float>(),
						Info.Value[1].get<float>(),
						Info.Value[2].get<float>(),
						Info.Value[3].get<float>()
					};
					ConstantBuffer->SetData(Value, sizeof(Value), Info.Offset);
				}
				else if (Info.Type == "float4x4" && Info.Value.is_array() && Info.Value.size() >= 16)
				{
					float Value[16] = {};
					for (int32 Index = 0; Index < 16; ++Index)
					{
						Value[Index] = Info.Value[Index].get<float>();
					}
					ConstantBuffer->SetData(Value, sizeof(Value), Info.Offset);
				}
			}
		}
	}
	// 경로 캐시에 등록한다.
	PathCache[InFilePath] = Material;

	if (Json.contains("Name"))
	{
		const FString Name = Json["Name"].get<FString>();
		Material->SetOriginName(Name);
		NameCache[Name] = Material;
	}

	return Material;
}

std::shared_ptr<FMaterial> FMaterialManager::FindByName(const FString& Name) const
{
	auto It = NameCache.find(Name);
	if (It != NameCache.end())
	{
		return It->second;
	}
	return nullptr;
}

void FMaterialManager::Register(const FString& Name, const std::shared_ptr<FMaterial>& InMaterial)
{
	if (InMaterial)
	{
		NameCache[Name] = InMaterial;
	}
}

TArray<FString> FMaterialManager::GetLoadedPaths() const
{
	TArray<FString> Result;
	for (const auto& Pair : PathCache)
	{
		Result.push_back(Pair.first);
	}
	return Result;
}

TArray<FString> FMaterialManager::GetAllMaterialNames() const
{
	TArray<FString> Names;
	for (const auto& Pair : NameCache)
	{
		Names.push_back(Pair.first);
	}
	return Names;
}

void FMaterialManager::Clear()
{
	PathCache.clear();
	NameCache.clear();
}
