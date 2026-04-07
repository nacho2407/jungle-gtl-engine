#include "MaterialManager.h"
#include "Renderer/RenderStateManager.h"
#include "Material.h"
#include "Shader.h"
#include "ShaderMap.h"
#include "Core/Paths.h"
#include "Debug/EngineLog.h"
#include "ThirdParty/nlohmann/json.hpp"
#include "Core/Engine.h"
#include "Renderer/Renderer.h"
#include <fstream>

// HLSL cbuffer 레이아웃을 계산할 때 사용하는 보조 함수들이다.

namespace
{
	// 타입 문자열을 바이트 크기로 변환한다.
	uint32 GetTypeSize(const FString& Type)
	{
		if (Type == "float")    return 4;
		if (Type == "float2")   return 8;
		if (Type == "float3")   return 12;
		if (Type == "float4")   return 16;
		if (Type == "float4x4") return 64;
		return 0;
	}

	// HLSL cbuffer 규칙에 맞춰 16바이트 경계를 넘지 않도록 오프셋을 정렬한다.
	uint32 AlignOffset(uint32 Offset, uint32 TypeSize)
	{
		// 현재 16바이트 블록에서 남은 공간을 계산한다.
		uint32 BoundaryStart = (Offset / 16) * 16;
		uint32 Remaining = BoundaryStart + 16 - Offset;

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
	auto MaterialDir = FPaths::MaterialDir();
	try {
		if (!fs::exists(MaterialDir) /* && fs::is_directory(FPaths::MaterialDir()) */)
		{
			UE_LOG("[MaterialManager] Material dir not exists\n");
			return;
		}

		// 모든 JSON 파일을 찾아 순차적으로 적재한다.
		for (const auto& entry : fs::directory_iterator(MaterialDir)) {
			if (entry.is_regular_file() && entry.path().extension() == ".json") {
				FString FilePath = FPaths::FromPath(entry.path());
				LoadFromFile(InDevice, InStateManager, FilePath);
			}
		}
	}
	catch (const fs::filesystem_error& ex) {
		UE_LOG("[MaterialManager] Filesystem Error while preload materials: %s\n", ex.what());
	}
}

std::shared_ptr<FMaterial> FMaterialManager::LoadFromFile(
	ID3D11Device* InDevice,
	FRenderStateManager* InStateManager,
	const FString& InFilePath
)
{
	// 동일한 경로를 다시 요청하면 캐시를 재사용한다.
	auto It = PathCache.find(InFilePath);
	if (It != PathCache.end())
	{
		return It->second;
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
	auto Mat = std::make_shared<FMaterial>();

	// 셰이더 경로를 읽어 필요한 버텍스/픽셀 셰이더를 연결한다.
	if (Json.contains("VertexShader"))
	{
		FString VSRelPath = Json["VertexShader"].get<FString>();
		std::wstring WVSPath = (FPaths::ProjectRoot() / FPaths::ToPath(VSRelPath)).wstring();
		auto VS = FShaderMap::Get().GetOrCreateVertexShader(InDevice, WVSPath.c_str());
		Mat->SetVertexShader(VS);
	}

	if (Json.contains("PixelShader"))
	{
		FString PSRelPath = Json["PixelShader"].get<FString>();
		std::wstring WPSPath = (FPaths::ProjectRoot() / FPaths::ToPath(PSRelPath)).wstring();
		auto PS = FShaderMap::Get().GetOrCreatePixelShader(InDevice, WPSPath.c_str());
		Mat->SetPixelShader(PS);
	}


	// Render State를 JSON 설정에 맞춰 구성한다.
	if (Json.contains("RenderState"))
	{
		auto RenderStatesJson = Json["RenderState"];

		FRasterizerStateOption rasterizerOption;
		if (RenderStatesJson.contains("FillMode"))
		{
			rasterizerOption.FillMode = RenderStatesJson["FillMode"].get<D3D11_FILL_MODE>();
		}
		if (RenderStatesJson.contains("CullMode"))
		{
			rasterizerOption.CullMode = RenderStatesJson["CullMode"].get<D3D11_CULL_MODE>();
		}
		auto RasterizerState = InStateManager->GetOrCreateRasterizerState(rasterizerOption);
		Mat->SetRasterizerOption(rasterizerOption);	// 머티리얼이 선택한 래스터라이저 옵션도 함께 보관한다.
		Mat->SetRasterizerState(RasterizerState);



		FDepthStencilStateOption depthStencilOption;
		if (RenderStatesJson.contains("DepthTest"))
		{
			depthStencilOption.DepthEnable = RenderStatesJson["DepthTest"].get<bool>();
		}
		else
		{
			depthStencilOption.DepthEnable = true;	// 기본값은 깊이 테스트 사용
		}
		if (RenderStatesJson.contains("DepthWrite"))
		{
			depthStencilOption.DepthWriteMask = RenderStatesJson["DepthWrite"].get<D3D11_DEPTH_WRITE_MASK>();
		}
		else
		{
			depthStencilOption.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; // 기본값은 깊이 쓰기 허용
		}
		if (RenderStatesJson.contains("StencilEnable"))
		{
			depthStencilOption.StencilEnable = RenderStatesJson["StencilEnable"].get<bool>();
		}
		else
		{
			depthStencilOption.StencilEnable = false; // 기본값은 스텐실 비활성화
		}
		if (RenderStatesJson.contains("StencilReadMask"))
		{
			depthStencilOption.StencilReadMask = RenderStatesJson["StencilReadMask"].get<uint8>();
		}
		if (RenderStatesJson.contains("StencilWriteMask"))
		{
			depthStencilOption.StencilWriteMask = RenderStatesJson["StencilWriteMask"].get<uint8>();
		}
		// 문자열로 저장된 깊이 비교 함수를 D3D11 비교 함수 enum으로 변환한다.
		if (RenderStatesJson.contains("DepthFunc"))
		{
			FString Func = RenderStatesJson["DepthFunc"].get<std::string>();
			if (Func == "LessEqual")
				depthStencilOption.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
			else if (Func == "Less")
				depthStencilOption.DepthFunc = D3D11_COMPARISON_LESS;
			else if (Func == "Always")
				depthStencilOption.DepthFunc = D3D11_COMPARISON_ALWAYS;
		}
		auto DepthStencilState = InStateManager->GetOrCreateDepthStencilState(depthStencilOption);
		Mat->SetDepthStencilOption(depthStencilOption);
		Mat->SetDepthStencilState(DepthStencilState);

		// DepthBias 같은 추가 상태 옵션이 필요해지면 여기서 확장한다.
	}

	if (Json.contains("Textures"))
	{
		auto TexturesJson = Json["Textures"];

		// "Diffuse" 슬롯에 지정된 텍스처를 머티리얼 텍스처로 로드한다.
		if (TexturesJson.contains("Diffuse"))
		{
			FString TexRelPath = TexturesJson["Diffuse"].get<FString>();
			std::filesystem::path FullTexPath = FPaths::AssetDir() / FPaths::ToPath(TexRelPath);

			ID3D11ShaderResourceView* NewSRV = nullptr;

			if (GEngine->GetRenderer()->CreateTextureFromSTB(
				InDevice,
				FullTexPath,
				&NewSRV))
			{
				// FMaterialTexture 래퍼를 만들어 머티리얼에 연결한다.
				auto MatTexture = std::make_shared<FMaterialTexture>();
				MatTexture->TextureSRV = NewSRV;
				Mat->SetMaterialTexture(MatTexture);

			}
		}
	}

	// 상수 버퍼 정의를 읽어 머티리얼 파라미터를 구성한다.
	if (Json.contains("ConstantBuffers"))
	{
		for (auto& CBJson : Json["ConstantBuffers"])
		{
			if (!CBJson.contains("Parameters"))
			{
				continue;
			}

			auto& Params = CBJson["Parameters"];

			// 1단계: 파라미터 타입과 정렬 규칙을 적용해 레이아웃을 계산한다.
			struct FParamInfo
			{
				FString Name;
				uint32 Offset;
				uint32 Size;
				FString Type;
				nlohmann::json Value;
			};
			TArray<FParamInfo> ParamList;
			uint32 CurrentOffset = 0;

			for (auto& P : Params)
			{
				FString Type = P.value("Type", "");
				uint32 TypeSize = GetTypeSize(Type);
				if (TypeSize == 0)
				{
					continue;
				}

				uint32 AlignedOffset = AlignOffset(CurrentOffset, TypeSize);

				FParamInfo Info;
				Info.Name = P.value("Name", "");
				Info.Offset = AlignedOffset;
				Info.Size = TypeSize;
				Info.Type = Type;
				Info.Value = P.contains("Value") ? P["Value"] : nlohmann::json();
				ParamList.push_back(Info);

				CurrentOffset = AlignedOffset + TypeSize;
			}

			if (CurrentOffset == 0)
			{
				continue;
			}

			uint32 BufferSize = AlignBufferSize(CurrentOffset);

			// 2단계: 계산된 크기로 상수 버퍼를 생성한다.
			int32 SlotIndex = Mat->CreateConstantBuffer(InDevice, BufferSize);
			if (SlotIndex < 0)
			{
				continue;
			}

			FMaterialConstantBuffer* CB = Mat->GetConstantBuffer(SlotIndex);

			// 이름이 있는 파라미터는 런타임 조회를 위해 등록한다.
			for (auto& Info : ParamList)
			{
				if (!Info.Name.empty())
				{
					Mat->RegisterParameter(Info.Name, SlotIndex, Info.Offset, Info.Size);
				}
			}

			// 3단계: 초기값이 있는 파라미터는 상수 버퍼에 바로 기록한다.
			for (auto& Info : ParamList)
			{
				if (Info.Value.is_null())
				{
					continue;
				}

				if (Info.Type == "float" && Info.Value.is_number())
				{
					float Val = Info.Value.get<float>();
					CB->SetData(&Val, sizeof(float), Info.Offset);
				}
				else if (Info.Type == "float2" && Info.Value.is_array() && Info.Value.size() >= 2)
				{
					float Val[2] = {
						Info.Value[0].get<float>(),
						Info.Value[1].get<float>()
					};
					CB->SetData(Val, sizeof(Val), Info.Offset);
				}
				else if (Info.Type == "float3" && Info.Value.is_array() && Info.Value.size() >= 3)
				{
					float Val[3] = {
						Info.Value[0].get<float>(),
						Info.Value[1].get<float>(),
						Info.Value[2].get<float>()
					};
					CB->SetData(Val, sizeof(Val), Info.Offset);
				}
				else if (Info.Type == "float4" && Info.Value.is_array() && Info.Value.size() >= 4)
				{
					float Val[4] = {
						Info.Value[0].get<float>(),
						Info.Value[1].get<float>(),
						Info.Value[2].get<float>(),
						Info.Value[3].get<float>()
					};
					CB->SetData(Val, sizeof(Val), Info.Offset);
				}
				else if (Info.Type == "float4x4" && Info.Value.is_array() && Info.Value.size() >= 16)
				{
					float Val[16];
					for (int32 i = 0; i < 16; ++i)
					{
						Val[i] = Info.Value[i].get<float>();
					}
					CB->SetData(Val, sizeof(Val), Info.Offset);
				}
			}
		}
	}

	// 경로 캐시에 등록한다.
	PathCache[InFilePath] = Mat;

	if (Json.contains("Name"))
	{
		FString Name = Json["Name"].get<FString>();
		Mat->SetOriginName(Name);
		NameCache[Name] = Mat;
	}

	return Mat;
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
	for (const auto& Pair: NameCache)
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
