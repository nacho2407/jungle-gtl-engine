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

// ??? HLSL cbuffer ?⑦궧 ?좏떥 ???

namespace
{
	// ??낅퀎 諛붿씠???ш린
	uint32 GetTypeSize(const FString& Type)
	{
		if (Type == "float")    return 4;
		if (Type == "float2")   return 8;
		if (Type == "float3")   return 12;
		if (Type == "float4")   return 16;
		if (Type == "float4x4") return 64;
		return 0;
	}

	// ??낅퀎 ?뺣젹 (HLSL ?⑦궧 猷? 16諛붿씠??寃쎄퀎瑜??섏쓣 ???놁쓬)
	uint32 AlignOffset(uint32 Offset, uint32 TypeSize)
	{
		// 16諛붿씠??寃쎄퀎 ???⑥? 怨듦컙
		uint32 BoundaryStart = (Offset / 16) * 16;
		uint32 Remaining = BoundaryStart + 16 - Offset;

		// ?⑥? 怨듦컙???ㅼ뼱媛吏 ?딆쑝硫??ㅼ쓬 16諛붿씠??寃쎄퀎濡?
		if (TypeSize > Remaining)
		{
			return BoundaryStart + 16;
		}
		return Offset;
	}

	// ?꾩껜 ?곸닔 踰꾪띁 ?ш린瑜?16諛붿씠?몃줈 ?뺣젹
	uint32 AlignBufferSize(uint32 Size)
	{
		return (Size + 15) & ~15;
	}
}

// ??? FMaterialManager ???

FMaterialManager& FMaterialManager::Get()
{
	static FMaterialManager Instance;
	return Instance;
}

void FMaterialManager::LoadAllMaterials(ID3D11Device* InDevice, FRenderStateManager* InStateManager)
{
	// 寃쎈줈 ?댁쓽 紐⑤뱺 癒명떚由ъ뼹 JSON ?뚯씪 媛?몄삤湲?
	namespace fs = std::filesystem;
	auto MaterialDir = FPaths::MaterialDir();
	try {
		if (!fs::exists(MaterialDir) /* && fs::is_directory(FPaths::MaterialDir()) */)
		{
			UE_LOG("[MaterialManager] Material dir not exists\n");
			return;
		}

		// ?꾨? 罹먯떆???깅줉
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
	// 寃쎈줈 罹먯떆 ?뺤씤
	auto It = PathCache.find(InFilePath);
	if (It != PathCache.end())
	{
		return It->second;
	}

	// JSON ?뚯씪 濡쒕뱶
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

	// Material ?앹꽦
	auto Mat = std::make_shared<FMaterial>();

	// ?곗씠??濡쒕뱶 (?꾨줈?앺듃 猷⑦듃 湲곗? ?곷? 寃쎈줈)
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


	// Render State 濡쒕뱶
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
		Mat->SetRasterizerOption(rasterizerOption);	// ?붾쾭源낆슜 ?뺣낫 ?쎌엯
		Mat->SetRasterizerState(RasterizerState);



		FDepthStencilStateOption depthStencilOption;
		if (RenderStatesJson.contains("DepthTest"))
		{
			depthStencilOption.DepthEnable = RenderStatesJson["DepthTest"].get<bool>();
		}
		else
		{
			depthStencilOption.DepthEnable = true;	// 湲곕낯媛?
		}
		if (RenderStatesJson.contains("DepthWrite"))
		{
			depthStencilOption.DepthWriteMask = RenderStatesJson["DepthWrite"].get<D3D11_DEPTH_WRITE_MASK>();
		}
		else
		{
			depthStencilOption.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; // 湲곕낯媛?
		}
		if (RenderStatesJson.contains("StencilEnable"))
		{
			depthStencilOption.StencilEnable = RenderStatesJson["StencilEnable"].get<bool>();
		}
		else
		{
			depthStencilOption.StencilEnable = false; // 湲곕낯媛?
		}
		if (RenderStatesJson.contains("StencilReadMask"))
		{
			depthStencilOption.StencilReadMask = RenderStatesJson["StencilReadMask"].get<uint8>();
		}
		if (RenderStatesJson.contains("StencilWriteMask"))
		{
			depthStencilOption.StencilWriteMask = RenderStatesJson["StencilWriteMask"].get<uint8>();
		}
		//DepthTest: true, DepthFunc: Less -> 湲곕낯 硫붿떆媛 ?대? 媛숈? 源딆씠瑜?湲곕줉?대넃??-> ?섏씠?쇱씠?멸? "??媛源뚯슫媛?" 鍮꾧탳 -> 媛숈? 源딆씠?덇퉴 ?듦낵 紐삵븿 -> ??洹몃젮吏?
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

		// DepthBias ?깆쓽 異붽? ?듭뀡??吏?먰븯?ㅻ㈃ ?ш린??異붽?
	}

	if (Json.contains("Textures"))
	{
		auto TexturesJson = Json["Textures"];

		// "Diffuse" ?щ’ ?띿뒪泥??뺤씤
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
				// FMaterialTexture 援ъ“泥??앹꽦 諛??μ갑
				auto MatTexture = std::make_shared<FMaterialTexture>();
				MatTexture->TextureSRV = NewSRV;
				Mat->SetMaterialTexture(MatTexture);

			}
		}
	}

	// ?곸닔 踰꾪띁 濡쒕뱶
	if (Json.contains("ConstantBuffers"))
	{
		for (auto& CBJson : Json["ConstantBuffers"])
		{
			if (!CBJson.contains("Parameters"))
			{
				continue;
			}

			auto& Params = CBJson["Parameters"];

			// 1李? ?ㅽ봽??怨꾩궛 + 珥??ш린 ?곗텧
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

			// 2李? ?곸닔 踰꾪띁 ?앹꽦
			int32 SlotIndex = Mat->CreateConstantBuffer(InDevice, BufferSize);
			if (SlotIndex < 0)
			{
				continue;
			}

			FMaterialConstantBuffer* CB = Mat->GetConstantBuffer(SlotIndex);

			// ?뚮씪誘명꽣 ?대쫫 ?깅줉
			for (auto& Info : ParamList)
			{
				if (!Info.Name.empty())
				{
					Mat->RegisterParameter(Info.Name, SlotIndex, Info.Offset, Info.Size);
				}
			}

			// 3李? 珥덇린媛?湲곕줉
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

	// 罹먯떆 ?깅줉
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
