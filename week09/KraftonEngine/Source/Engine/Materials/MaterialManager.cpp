#include "MaterialManager.h"
#include <filesystem>
#include <fstream>
#include "Materials/Material.h"
#include "Platform/Paths.h"
#include "Render/Resource/ShaderManager.h"

UMaterialInterface* FMaterialManager::CreateMaterial(const FString& MatFilePath)
{
	// 1. 캐시 반환
	auto It = InterfaceCache.find(MatFilePath);
	if (It != InterfaceCache.end())
	{
		return It->second;
	}

	// 2. 캐시에 없다면 JSON에서 읽기 
	json::JSON JsonData = ReadJsonFile(MatFilePath);
	if (JsonData.IsNull())
	{
		// 파일 없으면 기본 머티리얼 생성
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
	MaterialCache.emplace(PathFileName, Material);

	// 5. 파라미터 및 텍스처 적용
	ApplyParameters(Material, JsonData);
	ApplyTextures(Material, JsonData);

	// 6. UMaterialInterface 래퍼 
	UMaterialInterface* MatInterface = UObjectManager::Get().CreateObject<UMaterialInterface>();
	MatInterface->Material = Material;

	InterfaceCache.insert({ MatFilePath, MatInterface });
	return MatInterface;
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

	// Template으로부터 상수버퍼 레이아웃을 받아와 CBuffer 생성 후 UMaterial에 주입
	const auto& RequiredBuffers = Template->GetParameterInfo();

	// 이미 생성한 버퍼 이름을 기록하여 중복 생성을 막는 벡터(또는 Set)
	std::vector<FString> CreatedBuffers;

	for (const auto& BufferInfo : RequiredBuffers)
	{
		const FMaterialParameterInfo& ParamInfo = BufferInfo.second;

		// 1. 이미 생성한 버퍼("PerMaterial" 등)라면 이번 파라미터는 건너뜀
		if (std::find(CreatedBuffers.begin(), CreatedBuffers.end(), ParamInfo.BufferName) != CreatedBuffers.end())
		{
			continue;
		}

		// 1. D3D11 버퍼 생성 (Manager가 Device를 가지고 있으므로 여기서 수행)
		ID3D11Buffer* RawGPUBuffer = nullptr;
		D3D11_BUFFER_DESC desc = {};
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.ByteWidth = BufferInfo.second.Size;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		Device->CreateBuffer(&desc, nullptr, &RawGPUBuffer);

		// 2. FMaterialConstantBuffer 래퍼 객체 생성 및 초기화
		auto MatCB = std::make_unique<FMaterialConstantBuffer>();
		MatCB->Init(RawGPUBuffer, BufferInfo.second.Size, BufferInfo.second.SlotIndex); // 설계하신 Init 호출

		// 3. 주입할 맵에 추가
		InjectedBuffers.emplace(BufferInfo.second.BufferName, std::move(MatCB));

		CreatedBuffers.push_back(ParamInfo.BufferName);
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


ERenderPass FMaterialManager::StringToRenderPass(const FString& RenderPassStr) const
{
	if (RenderPassStr == "Opaque")        return ERenderPass::Opaque;
	if (RenderPassStr == "Font")          return ERenderPass::Font;
	if (RenderPassStr == "SubUV")         return ERenderPass::SubUV;
	if (RenderPassStr == "Billboard")     return ERenderPass::Billboard;
	if (RenderPassStr == "Translucent")   return ERenderPass::Translucent;
	if (RenderPassStr == "SelectionMask") return ERenderPass::SelectionMask;
	if (RenderPassStr == "Editor")        return ERenderPass::Editor;
	if (RenderPassStr == "Grid")          return ERenderPass::Grid;
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

	auto [It, bInserted] = TemplateCache.emplace(ShaderPath, FMaterialTemplate());

	if (bInserted)
	{
		It->second->Create(Shader, RenderPass);
	}

	return It->second;
}

