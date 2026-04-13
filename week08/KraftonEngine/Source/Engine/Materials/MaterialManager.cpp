#include "MaterialManager.h"
#include <filesystem>
#include <fstream>
#include "Materials/Material.h"
#include "Platform/Paths.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Resource/Buffer.h"
#include "Texture/Texture2D.h"

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
		FMaterialTemplate* Template = GetOrCreateTemplate(DefaultShaderPath, ERenderPass::Opaque);
		TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> Buffers = CreateConstantBuffers(Template);
		DefaultMaterial->Create(MatFilePath, Template, std::move(Buffers));
		// DiffuseColor 기본값 세팅
		DefaultMaterial->SetVector4Parameter("DiffuseColor", FVector4(1.0f, 0.0f, 1.0f, 1.0f)); // 핑크
		MaterialCache.emplace(MatFilePath, DefaultMaterial);
		return DefaultMaterial;
	}

	// 3. JSON에서 기본 정보 추출
	FString PathFileName = JsonData["PathFileName"].ToString().c_str();
	FString ShaderPath = JsonData["ShaderPath"].ToString().c_str();
	FString RenderPassStr = JsonData["RenderPass"].ToString().c_str();
	ERenderPass RenderPass = StringToRenderPass(RenderPassStr);

	// 4. 템플릿 확보 (없으면 리플렉션을 통해 생성됨)
	FMaterialTemplate* Template = GetOrCreateTemplate(ShaderPath, RenderPass);
	if (!Template) return nullptr;

	// 3. D3D 상수 버퍼 생성
	auto InjectedBuffers = CreateConstantBuffers(Template);

	// 4. UMaterial 인스턴스 생성 및 초기화
	UMaterial* Material = UObjectManager::Get().CreateObject<UMaterial>();
	Material->Create(PathFileName, Template, std::move(InjectedBuffers));
	MaterialCache.emplace(MatFilePath, Material);

	// 5. 파라미터 및 텍스처 적용
	ApplyParameters(Material, JsonData);
	ApplyTextures(Material, JsonData);

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
			Material->SetTextureParameter("DiffuseTexture", Texture);
		}
	}
}


ERenderPass FMaterialManager::StringToRenderPass(const FString& RenderPassStr) const
{
	if (RenderPassStr == "Opaque")        return ERenderPass::Opaque;
	if (RenderPassStr == "AlphaBlend")    return ERenderPass::AlphaBlend;
	if (RenderPassStr == "Decal")         return ERenderPass::Decal;
	if (RenderPassStr == "SelectionMask") return ERenderPass::SelectionMask;
	if (RenderPassStr == "EditorLines")   return ERenderPass::EditorLines;
	if (RenderPassStr == "PostProcess")   return ERenderPass::PostProcess;
	if (RenderPassStr == "GizmoOuter")    return ERenderPass::GizmoOuter;
	if (RenderPassStr == "GizmoInner")    return ERenderPass::GizmoInner;
	if (RenderPassStr == "OverlayFont")   return ERenderPass::OverlayFont;

	// 매칭되는 게 없으면 기본값 반환
	return ERenderPass::Opaque;
}

FMaterialTemplate* FMaterialManager::GetOrCreateTemplate(const FString& ShaderPath, ERenderPass RenderPass)
{
	
	// 1. 템플릿이 캐시에 있는지 확인
	// (셰이더 경로를 키값으로 사용)
	auto It = TemplateCache.find(ShaderPath);
	if (It != TemplateCache.end())
	{
		return It->second; // 이미 누군가 만들어둔 게 있으면 즉시 반환!
	}

	// 2. 템플릿이 기존에 없다면 새로 제작
	// - 셰이더 파일 읽고 컴파일
	FShader* Shader = FShaderManager::Get().CreateCustomShader(Device, FPaths::ToWide(ShaderPath).c_str());

	if (!Shader)
	{
		return nullptr; // 셰이더 로드 실패
	}

	FMaterialTemplate* NewTemplate = new FMaterialTemplate();
	NewTemplate->Create(Shader, RenderPass);
	TemplateCache.emplace(ShaderPath, NewTemplate);
	return NewTemplate;
}

