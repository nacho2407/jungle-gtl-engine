#include "MaterialManager.h"
#include <filesystem>
#include <fstream>
#include "Materials/Material.h"
#include "Platform/Paths.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Resource/Buffer.h"
#include "Texture/Texture2D.h"
#include "Engine/Platform/Paths.h"
#include "Render/Pipeline/Renderer.h"

void FMaterialManager::ScanMaterialAssets()
{
	AvailableMaterialFiles.clear();

	const std::filesystem::path MaterialRoot = FPaths::RootDir() + L"Asset\\Materials\\";

	if (!std::filesystem::exists(MaterialRoot))
	{
		return;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(MaterialRoot))
	{
		if (!Entry.is_regular_file()) continue;

		const std::filesystem::path& Path = Entry.path();

		// 확장자가 .json인지 확인
		if (Path.extension() != L".json") continue;
		if (Path.stem() == L"None") continue; // Fallback 머티리얼은 목록에서 제외

		FMaterialAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Path.stem().wstring());
		Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
		AvailableMaterialFiles.push_back(std::move(Item));
	}
}



UMaterial* FMaterialManager::GetOrCreateMaterial(const FString& MatFilePath)
{
	// 1. 캐시 반환
	auto It = MaterialCache.find(MatFilePath);
	if (It != MaterialCache.end())
	{
		return It->second;
	}

	// 2. 캐시에 없다면 JSON에서 읽기 
	json::JSON JsonData = ReadJsonFile(MatFilePath);
	if (JsonData.IsNull())
	{
		// 기본 머티리얼 생성
		UMaterial* DefaultMaterial = UObjectManager::Get().CreateObject<UMaterial>();
		FMaterialTemplate* Template = GetOrCreateTemplate(DefaultShaderPath);
		TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> Buffers = CreateConstantBuffers(Template);
		DefaultMaterial->Create(MatFilePath, Template, ERenderPass::Opaque, EBlendState::Opaque, EDepthStencilState::Default, ERasterizerState::SolidBackCull, std::move(Buffers));
		// 폴백: 핑크색으로 미지정 머티리얼임을 표시
		DefaultMaterial->SetVector4Parameter("SectionColor", FVector4(1.0f, 0.0f, 1.0f, 1.0f));
		MaterialCache.emplace(MatFilePath, DefaultMaterial);
		return DefaultMaterial;
	}

	// 3. JSON에서 기본 정보 추출
	FString PathFileName = JsonData["PathFileName"].ToString().c_str();
	FString ShaderPath = JsonData["ShaderPath"].ToString().c_str();
	FString RenderPassStr = JsonData["RenderPass"].ToString().c_str();
	ERenderPass RenderPass = StringToRenderPass(RenderPassStr);

	// 새로운 렌더 상태 추출 (JSON에 없으면 패스 기반 기본값)
	FString BlendStr = JsonData.hasKey("BlendState") ? JsonData["BlendState"].ToString().c_str() : "";
	FString DepthStr = JsonData.hasKey("DepthStencilState") ? JsonData["DepthStencilState"].ToString().c_str() : "";
	FString RasterStr = JsonData.hasKey("RasterizerState") ? JsonData["RasterizerState"].ToString().c_str() : "";

	EBlendState BlendState = StringToBlendState(BlendStr, RenderPass);
	EDepthStencilState DepthState = StringToDepthStencilState(DepthStr, RenderPass);
	ERasterizerState RasterState = StringToRasterizerState(RasterStr, RenderPass);

	// 4. 템플릿 확보 (없으면 리플렉션을 통해 생성됨)
	FMaterialTemplate* Template = GetOrCreateTemplate(ShaderPath);
	if (!Template) return nullptr;

	// 5. D3D 상수 버퍼 생성
	auto InjectedBuffers = CreateConstantBuffers(Template);

	// 6. UMaterial 인스턴스 생성 및 초기화 (RenderPass는 인스턴스별)
	UMaterial* Material = UObjectManager::Get().CreateObject<UMaterial>();
	Material->Create(PathFileName, Template, RenderPass, BlendState, DepthState, RasterState, std::move(InjectedBuffers));
	MaterialCache.emplace(MatFilePath, Material);

	//템플릿을 통해 material에 넣기
	bool bInjected = InjectDefaultParameters(JsonData, Template, Material);

	// 5. 파라미터 및 텍스처 적용
	ApplyParameters(Material, JsonData);
	ApplyTextures(Material, JsonData);

	// JSON 데이터에도 현재 상태를 기록 (나중에 저장 시 유지되도록)
	JsonData["BlendState"] = BlendStr.empty() ? "" : BlendStr.c_str();
	JsonData["DepthStencilState"] = DepthStr.empty() ? "" : DepthStr.c_str();
	JsonData["RasterizerState"] = RasterStr.empty() ? "" : RasterStr.c_str();

	//최종적으로 material 저장
	SaveToJSON(JsonData, MatFilePath);

	return Material;
}

json::JSON FMaterialManager::ReadJsonFile(const FString& FilePath) const
{
	std::ifstream File(FPaths::ToWide(FilePath).c_str());
	if (!File.is_open()) return json::JSON(); // Null JSON 반환

	std::stringstream Buffer;
	Buffer << File.rdbuf();
	return json::JSON::Load(Buffer.str());
}

TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> FMaterialManager::CreateConstantBuffers(FMaterialTemplate* Template)
{
	
	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> InjectedBuffers;

	const auto& RequiredBuffers = Template->GetParameterInfo();
	std::vector<FString> CreatedBuffers;

	for (const auto& BufferInfo : RequiredBuffers)
	{
		const FMaterialParameterInfo* ParamInfo = BufferInfo.second;

		if (std::find(CreatedBuffers.begin(), CreatedBuffers.end(), ParamInfo->BufferName) != CreatedBuffers.end())
			continue;

		auto MatCB = std::make_unique<FMaterialConstantBuffer>();
		MatCB->Init(Device, ParamInfo->BufferSize, ParamInfo->SlotIndex);

		InjectedBuffers.emplace(ParamInfo->BufferName, std::move(MatCB));
		CreatedBuffers.push_back(ParamInfo->BufferName);
	}

	return InjectedBuffers;
}

void FMaterialManager::ApplyParameters(UMaterial* Material, json::JSON& JsonData)
{
	if (!JsonData.hasKey("Parameters")) return;

	for (auto& Pair : JsonData["Parameters"].ObjectRange())
	{
		FString ParamName = Pair.first.c_str();
		json::JSON& Value = Pair.second;

		if (Value.JSONType() == json::JSON::Class::Array)
		{
			if (Value.length() == 3)
			{
				Material->SetVector3Parameter(ParamName, FVector(Value[0].ToFloat(), Value[1].ToFloat(), Value[2].ToFloat()));
			}
			else if (Value.length() == 4)
			{
				Material->SetVector4Parameter(ParamName, FVector4(Value[0].ToFloat(), Value[1].ToFloat(), Value[2].ToFloat(), Value[3].ToFloat()));
			}
		}
		else if (Value.JSONType() == json::JSON::Class::Floating || Value.JSONType() == json::JSON::Class::Integral)
		{
			Material->SetScalarParameter(ParamName, Value.ToFloat());
		}
	}
}

void FMaterialManager::ApplyTextures(UMaterial* Material, json::JSON& JsonData)
{
	if (!JsonData.hasKey("Textures")) return;

	for (auto& Pair : JsonData["Textures"].ObjectRange())
	{
		FString SlotName = Pair.first.c_str();
		FString TexturePath = Pair.second.ToString().c_str();

		UTexture2D* Texture = UTexture2D::LoadFromFile(TexturePath, Device);
		if (Texture)
		{
			Material->SetTextureParameter(SlotName, Texture);
		}
	}
}


ERenderPass FMaterialManager::StringToRenderPass(const FString& RenderPassStr) const
{
	if (RenderPassStr == "Opaque")        return ERenderPass::Opaque;
	if (RenderPassStr == "AlphaBlend")    return ERenderPass::AlphaBlend;
	if (RenderPassStr == "Decal")         return ERenderPass::Decal;
	if (RenderPassStr == "AdditiveDecal") return ERenderPass::AdditiveDecal;
	if (RenderPassStr == "SelectionMask") return ERenderPass::SelectionMask;
	if (RenderPassStr == "EditorLines")   return ERenderPass::EditorLines;
	if (RenderPassStr == "PostProcess")   return ERenderPass::PostProcess;
	if (RenderPassStr == "GizmoOuter")    return ERenderPass::GizmoOuter;
	if (RenderPassStr == "GizmoInner")    return ERenderPass::GizmoInner;
	if (RenderPassStr == "OverlayFont")   return ERenderPass::OverlayFont;

	// 매칭되는 게 없으면 기본값 반환
	return ERenderPass::Opaque;
}

EBlendState FMaterialManager::StringToBlendState(const FString& Str, ERenderPass Pass) const
{
	if (Str == "Opaque") return EBlendState::Opaque;
	if (Str == "AlphaBlend") return EBlendState::AlphaBlend;
	if (Str == "Additive") return EBlendState::Additive;
	if (Str == "NoColor") return EBlendState::NoColor;

	// Default by Pass
	if (Pass == ERenderPass::AlphaBlend || Pass == ERenderPass::Decal || Pass == ERenderPass::EditorLines || Pass == ERenderPass::PostProcess || Pass == ERenderPass::GizmoInner || Pass == ERenderPass::OverlayFont)
		return EBlendState::AlphaBlend;
	if (Pass == ERenderPass::AdditiveDecal)
		return EBlendState::Additive;
	if (Pass == ERenderPass::SelectionMask)
		return EBlendState::NoColor;

	return EBlendState::Opaque;
}

EDepthStencilState FMaterialManager::StringToDepthStencilState(const FString& Str, ERenderPass Pass) const
{
	if (Str == "Default") return EDepthStencilState::Default;
	if (Str == "DepthReadOnly") return EDepthStencilState::DepthReadOnly;
	if (Str == "StencilWrite") return EDepthStencilState::StencilWrite;
	if (Str == "StencilWriteOnlyEqual") return EDepthStencilState::StencilWriteOnlyEqual;
	if (Str == "NoDepth") return EDepthStencilState::NoDepth;
	if (Str == "GizmoInside") return EDepthStencilState::GizmoInside;
	if (Str == "GizmoOutside") return EDepthStencilState::GizmoOutside;

	// Default by Pass
	if (Pass == ERenderPass::Decal || Pass == ERenderPass::AdditiveDecal)
		return EDepthStencilState::DepthReadOnly;
	if (Pass == ERenderPass::SelectionMask)
		return EDepthStencilState::StencilWrite;
	if (Pass == ERenderPass::PostProcess || Pass == ERenderPass::OverlayFont)
		return EDepthStencilState::NoDepth;
	if (Pass == ERenderPass::GizmoOuter)
		return EDepthStencilState::GizmoOutside;
	if (Pass == ERenderPass::GizmoInner)
		return EDepthStencilState::GizmoInside;

	return EDepthStencilState::Default;
}

ERasterizerState FMaterialManager::StringToRasterizerState(const FString& Str, ERenderPass Pass) const
{
	if (Str == "SolidBackCull") return ERasterizerState::SolidBackCull;
	if (Str == "SolidFrontCull") return ERasterizerState::SolidFrontCull;
	if (Str == "SolidNoCull") return ERasterizerState::SolidNoCull;
	if (Str == "WireFrame") return ERasterizerState::WireFrame;

	// Default by Pass
	if (Pass == ERenderPass::Decal || Pass == ERenderPass::AdditiveDecal || Pass == ERenderPass::SelectionMask || Pass == ERenderPass::PostProcess)
		return ERasterizerState::SolidNoCull;

	return ERasterizerState::SolidBackCull;
}

void FMaterialManager::SaveToJSON(json::JSON& JsonData, const FString& MatFilePath)
{
	std::ofstream File(FPaths::ToWide(MatFilePath));
	File << JsonData.dump();
}

bool FMaterialManager::InjectDefaultParameters(json::JSON& JsonData, FMaterialTemplate* Template, UMaterial* Material)
{
	const auto& Layout = Template->GetParameterInfo();
	bool bInjected = false;

	for (const auto& Pair : Layout)
	{
		const FString& ParamName = Pair.first;
		const FMaterialParameterInfo* Info = Pair.second;

		// 이미 JSON에 있으면 스킵
		if (!JsonData["Parameters"][ParamName].IsNull())
			continue;

		bInjected = true;

		switch (Info->Size)
		{
			case sizeof(float) : // 4바이트 - Scalar
			{
				float Value = 0.f;
				Material->GetScalarParameter(ParamName, Value);
				JsonData["Parameters"][ParamName] = Value;
				break;
			}
			case sizeof(float) * 3: // 12바이트 - Vector3
			{
				FVector Value;
				Material->GetVector3Parameter(ParamName, Value);
				JsonData["Parameters"][ParamName] = json::Array(Value.X, Value.Y, Value.Z);
				break;
			}
			case sizeof(float) * 4: // 16바이트 - Vector4
			{
				FVector4 Value;
				Material->GetVector4Parameter(ParamName, Value);
				JsonData["Parameters"][ParamName] = json::Array(Value.X, Value.Y, Value.Z, Value.W);
				break;
			}
			case sizeof(float) * 16: // 64바이트 - Matrix
			{
				FMatrix Value;
				Material->GetMatrixParameter(ParamName, Value);
				auto MatArray = json::Array();
				for (int i = 0; i < 16; ++i)
					MatArray.append(Value.Data[i]);
				JsonData["Parameters"][ParamName] = MatArray;
				break;
			}
			default:
				break; // uint, bool 등 특수 케이스는 별도 처리 필요
		}
	}

	return bInjected;
}

FMaterialTemplate* FMaterialManager::GetOrCreateTemplate(const FString& ShaderPath)
{
	// 1. 템플릿이 캐시에 있는지 확인 (셰이더 경로를 키값으로 사용)
	auto It = TemplateCache.find(ShaderPath);
	if (It != TemplateCache.end())
	{
		return It->second;
	}

	// 2. 템플릿이 기존에 없다면 새로 제작 — 셰이더 파일 읽고 컴파일
	FShader* Shader = FShaderManager::Get().CreateCustomShader(Device, FPaths::ToWide(ShaderPath).c_str());
	if (!Shader)
	{
		return nullptr;
	}

	FMaterialTemplate* NewTemplate = new FMaterialTemplate();
	NewTemplate->Create(Shader);
	TemplateCache.emplace(ShaderPath, NewTemplate);
	return NewTemplate;
}

FMaterialManager::~FMaterialManager()
{
	if(!Device)
	{
		Release();
	}
	
}

void FMaterialManager::Release()
{
	// 1. TemplateCache 메모리 해제
	// GetOrCreateTemplate()에서 new FMaterialTemplate()로 직접 할당했으므로 여기서 delete 해줍니다.
	for (auto& Pair : TemplateCache)
	{
		if (Pair.second != nullptr)
		{
			delete Pair.second;
			Pair.second = nullptr;
		}
	}
	TemplateCache.clear();

	// 2. MaterialCache — UMaterial은 UObjectManager가 수명을 관리하므로 캐시 맵만 비움
	MaterialCache.clear();

	// 3. Device 참조 해제
	// 외부에서 주입받은 리소스이므로 포인터만 초기화합니다.
	Device = nullptr;
}