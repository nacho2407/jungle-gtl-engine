#include "MaterialManager.h"
#include <filesystem>
#include <fstream>
#include "Materials/Material.h"
#include "Platform/Paths.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Resource/Buffer.h"
#include "Texture/Texture2D.h"
#include "Render/Pipeline/Renderer.h"
#include "Object/ReferenceCollector.h"

namespace
{
	float ReadJsonNumber(const json::JSON& Value, float DefaultValue)
	{
		bool bOk = false;
		const double FloatValue = Value.ToFloat(bOk);
		if (bOk)
		{
			return static_cast<float>(FloatValue);
		}

		const long IntValue = Value.ToInt(bOk);
		return bOk ? static_cast<float>(IntValue) : DefaultValue;
	}

	bool ReadJsonBool(const json::JSON& Value, bool bDefaultValue)
	{
		bool bOk = false;
		const bool bBoolValue = Value.ToBool(bOk);
		if (bOk)
		{
			return bBoolValue;
		}

		const long IntValue = Value.ToInt(bOk);
		return bOk ? IntValue != 0 : bDefaultValue;
	}

	FVector4 ReadJsonVector4(json::JSON& Value, const FVector4& DefaultValue)
	{
		if (Value.JSONType() != json::JSON::Class::Array || Value.length() < 3)
		{
			return DefaultValue;
		}

		return FVector4(
			ReadJsonNumber(Value[0], DefaultValue.X),
			ReadJsonNumber(Value[1], DefaultValue.Y),
			ReadJsonNumber(Value[2], DefaultValue.Z),
			Value.length() >= 4 ? ReadJsonNumber(Value[3], DefaultValue.W) : DefaultValue.W);
	}

	void WriteMaterialBloomSettings(json::JSON& JsonData, const UMaterial* Material)
	{
		if (!Material)
		{
			return;
		}

		const FVector4 Color = Material->GetEmissiveColor();
		JsonData[MatKeys::EmissiveColor] = json::Array(Color.X, Color.Y, Color.Z, Color.W);
		JsonData[MatKeys::EmissiveIntensity] = Material->GetEmissiveIntensity();
		JsonData[MatKeys::bEnableBloom] = Material->IsBloomEnabled();
	}
}

void FMaterialManager::ScanMaterialAssets()
{
	AvailableMaterialFiles.clear();

	const std::filesystem::path MaterialRoot = FPaths::RootDir() + L"Content/Material/";

	if (!std::filesystem::exists(MaterialRoot))
	{
		return;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(MaterialRoot))
	{
		if (!Entry.is_regular_file()) continue;

		const std::filesystem::path& Path = Entry.path();

		const auto Ext = Path.extension();
		if (Ext != L".mat" && Ext != L".matinst") continue;
		if (Path.stem() == L"None") continue; // Fallback 머티리얼은 목록에서 제외

		FMaterialAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Path.stem().wstring());
		Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
		AvailableMaterialFiles.push_back(std::move(Item));
	}
}

UMaterial* FMaterialManager::GetOrCreateMaterial(const FString& MatFilePath)
{
	std::filesystem::path Path(FPaths::ToWide(MatFilePath));
	FString GenericPath = FPaths::ToUtf8(Path.generic_wstring());
	// 1. 캐시 반환
	auto It = MaterialCache.find(GenericPath);
	if (It != MaterialCache.end())
	{
		return It->second;
	}

	// 2. 캐시에 없다면 JSON에서 읽기 
	json::JSON JsonData = ReadJsonFile(GenericPath);
	if (JsonData.IsNull())
	{
		// 기본 머티리얼 생성
		UMaterial* DefaultMaterial = UObjectManager::Get().CreateObject<UMaterial>();
		FMaterialTemplate* Template = GetOrCreateTemplate(DefaultShaderPath);
		TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> Buffers = CreateConstantBuffers(Template);
		DefaultMaterial->Create(GenericPath, Template, ERenderPass::Opaque, EBlendState::Opaque, EDepthStencilState::Default, ERasterizerState::SolidBackCull, std::move(Buffers));
		// 폴백: 핑크색으로 미지정 머티리얼임을 표시
		DefaultMaterial->SetVector4Parameter("SectionColor", FVector4(1.0f, 0.0f, 1.0f, 1.0f));
		MaterialCache.emplace(GenericPath, DefaultMaterial);
		return DefaultMaterial;
	}

	// 3. JSON에서 기본 정보 추출
	bool bMaterialSchemaInjected = false;

	FString PathFileName = JsonData[MatKeys::PathFileName].ToString().c_str();
	if (PathFileName.empty())
	{
		PathFileName = GenericPath;
		JsonData[MatKeys::PathFileName] = PathFileName.c_str();
		bMaterialSchemaInjected = true;
	}

	FString ShaderPath = JsonData[MatKeys::ShaderPath].ToString().c_str();
	if (ShaderPath.empty())
	{
		ShaderPath = DefaultShaderPath;
		JsonData[MatKeys::ShaderPath] = ShaderPath.c_str();
		bMaterialSchemaInjected = true;
	}

	if (!JsonData.hasKey(MatKeys::Parameters) || JsonData[MatKeys::Parameters].JSONType() != json::JSON::Class::Object)
	{
		JsonData[MatKeys::Parameters] = json::JSON::Make(json::JSON::Class::Object);
		bMaterialSchemaInjected = true;
	}
	if (!JsonData.hasKey(MatKeys::Textures) || JsonData[MatKeys::Textures].JSONType() != json::JSON::Class::Object)
	{
		JsonData[MatKeys::Textures] = json::JSON::Make(json::JSON::Class::Object);
		bMaterialSchemaInjected = true;
	}

	FString RenderPassStr = JsonData[MatKeys::RenderPass].ToString().c_str();
	ERenderPass RenderPass = StringToRenderPass(RenderPassStr);

	// 새로운 렌더 상태 추출 (JSON에 없으면 패스 기반 기본값)
	FString BlendStr = JsonData.hasKey(MatKeys::BlendState) ? JsonData[MatKeys::BlendState].ToString().c_str() : "";
	FString DepthStr = JsonData.hasKey(MatKeys::DepthStencilState) ? JsonData[MatKeys::DepthStencilState].ToString().c_str() : "";
	FString RasterStr = JsonData.hasKey(MatKeys::RasterizerState) ? JsonData[MatKeys::RasterizerState].ToString().c_str() : "";

	EBlendState BlendState = StringToBlendState(BlendStr, RenderPass);
	EDepthStencilState DepthState = StringToDepthStencilState(DepthStr, RenderPass);
	ERasterizerState RasterState = StringToRasterizerState(RasterStr, RenderPass);

	FString ShadowModeStr = JsonData.hasKey(MatKeys::ShadowMode) ? JsonData[MatKeys::ShadowMode].ToString().c_str() : "Opaque";
	EMaterialShadowMode ShadowMode = StringToShadowMode(ShadowModeStr);
	const bool bHadEmissiveColor = JsonData.hasKey(MatKeys::EmissiveColor);
	const bool bHadEmissiveIntensity = JsonData.hasKey(MatKeys::EmissiveIntensity);
	const bool bHadEnableBloom = JsonData.hasKey(MatKeys::bEnableBloom);
	const bool bMaterialSettingsInjected = !bHadEmissiveColor || !bHadEmissiveIntensity || !bHadEnableBloom;

	FVector4 EmissiveColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	float EmissiveIntensity = 0.0f;
	bool bEnableBloom = false;
	if (bHadEmissiveColor)
	{
		EmissiveColor = ReadJsonVector4(JsonData[MatKeys::EmissiveColor], EmissiveColor);
	}
	if (bHadEmissiveIntensity)
	{
		EmissiveIntensity = ReadJsonNumber(JsonData[MatKeys::EmissiveIntensity], EmissiveIntensity);
	}
	if (bHadEnableBloom)
	{
		bEnableBloom = ReadJsonBool(JsonData[MatKeys::bEnableBloom], bEnableBloom);
	}

	// 4. 템플릿 확보 (없으면 리플렉션을 통해 생성됨)
	FMaterialTemplate* Template = GetOrCreateTemplate(ShaderPath);
	if (!Template) return nullptr;

	// 5. D3D 상수 버퍼 생성
	auto InjectedBuffers = CreateConstantBuffers(Template);

	// 6. UMaterial 인스턴스 생성 및 초기화 (RenderPass는 인스턴스별)
	UMaterial* Material = UObjectManager::Get().CreateObject<UMaterial>();
	Material->Create(PathFileName, Template, RenderPass, BlendState, DepthState, RasterState, std::move(InjectedBuffers));
	Material->SetShadowMode(ShadowMode);
	Material->SetEmissiveColor(EmissiveColor);
	Material->SetEmissiveIntensity(EmissiveIntensity);
	Material->SetBloomEnabled(bEnableBloom);
	MaterialCache.emplace(GenericPath, Material);

	//템플릿을 통해 material에 넣기
	bool bInjected = InjectDefaultParameters(JsonData, Template, Material);

	// 이전 셰이더의 찌꺼기 파라미터 정리
	bool bPurged = PurgeStaleParameters(JsonData, Template);

	// 5. 파라미터 및 텍스처 적용
	ApplyParameters(Material, JsonData);
	ApplyTextures(Material, JsonData);
	Material->RebuildCachedSRVs();

	using namespace RenderStateStrings;

	// JSON 데이터에도 현재 상태를 기록 (나중에 저장 시 유지되도록)
	JsonData[MatKeys::RenderPass] = ToString(RenderPassMap, RenderPass);
	JsonData[MatKeys::BlendState] = ToString(BlendStateMap, BlendState);
	JsonData[MatKeys::DepthStencilState] = ToString(DepthStencilStateMap, DepthState);
	JsonData[MatKeys::RasterizerState] = ToString(RasterizerStateMap, RasterState);
	JsonData[MatKeys::ShadowMode] = ShadowModeToString(ShadowMode);

	WriteMaterialBloomSettings(JsonData, Material);

	//최종적으로 material 저장
	if (bInjected || bPurged || bMaterialSettingsInjected || bMaterialSchemaInjected)
	{
		SaveToJSON(JsonData, GenericPath);
	}

	return Material;
}

UMaterialInstance* FMaterialManager::GetOrCreateMaterialInstance(const FString& MatInstFilePath)
{
	std::filesystem::path Path(FPaths::ToWide(MatInstFilePath));
	FString GenericPath = FPaths::ToUtf8(Path.generic_wstring());

	// 1. 캐시 반환
	auto It = MaterialInstanceCache.find(GenericPath);
	if (It != MaterialInstanceCache.end())
	{
		return It->second;
	}

	json::JSON JsonData = ReadJsonFile(GenericPath);
	if (JsonData.IsNull())
	{
		return nullptr;
	}

	FString ParentMaterialPath = JsonData[MatKeys::ParentMaterial].ToString().c_str();
	if (ParentMaterialPath.empty())
	{
		return nullptr;
	}

	UMaterial* ParentMaterial = GetOrCreateMaterial(ParentMaterialPath);
	if (!ParentMaterial)
	{
		return nullptr;
	}

	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> Buffers;
	if (ParentMaterial->GetTemplate())
	{
		Buffers = CreateConstantBuffers(ParentMaterial->GetTemplate());
	}

	UMaterialInstance* MaterialInstance = UObjectManager::Get().CreateObject<UMaterialInstance>();
	MaterialInstance->Create(GenericPath, ParentMaterial, std::move(Buffers));
	MaterialInstanceCache.emplace(GenericPath, MaterialInstance);

	ApplyParameters(MaterialInstance, JsonData);
	ApplyTextures(MaterialInstance, JsonData);
	ApplyBloomOverrides(MaterialInstance, JsonData);

	return MaterialInstance;
}

UMaterialInterface* FMaterialManager::GetOrCreateMaterialInterface(const FString& AssetPath)
{
	if (AssetPath.empty() || AssetPath == "None")
	{
		return nullptr;
	}

	std::filesystem::path Path(FPaths::ToWide(AssetPath));
	FString GenericPath = FPaths::ToUtf8(Path.generic_wstring());

	FString Extension = FPaths::ToUtf8(Path.extension().wstring());

	if (Extension == ".mat")
	{
		return GetOrCreateMaterial(GenericPath);
	}

	if (Extension == ".matinst")
	{
		return GetOrCreateMaterialInstance(GenericPath);
	}

	// 확장자가 없거나 legacy path면 우선 기존 material로 처리
	// 필요하면 여기서 .mat -> .matinst fallback 탐색도 가능
	return GetOrCreateMaterial(GenericPath);
}

bool FMaterialManager::ReloadMaterial(const FString& MatFilePath)
{
	std::filesystem::path Path(FPaths::ToWide(MatFilePath));
	FString GenericPath = FPaths::ToUtf8(Path.generic_wstring());

	auto It = MaterialCache.find(GenericPath);
	if (It == MaterialCache.end() || !It->second)
	{
		return false;
	}

	json::JSON JsonData = ReadJsonFile(GenericPath);
	if (JsonData.IsNull())
	{
		return false;
	}

	FString PathFileName = JsonData[MatKeys::PathFileName].ToString().c_str();
	if (PathFileName.empty())
	{
		PathFileName = GenericPath;
	}

	FString ShaderPath = JsonData[MatKeys::ShaderPath].ToString().c_str();
	if (ShaderPath.empty())
	{
		ShaderPath = DefaultShaderPath;
	}
	FString RenderPassStr = JsonData[MatKeys::RenderPass].ToString().c_str();
	ERenderPass RenderPass = StringToRenderPass(RenderPassStr);

	FString BlendStr = JsonData.hasKey(MatKeys::BlendState) ? JsonData[MatKeys::BlendState].ToString().c_str() : "";
	FString DepthStr = JsonData.hasKey(MatKeys::DepthStencilState) ? JsonData[MatKeys::DepthStencilState].ToString().c_str() : "";
	FString RasterStr = JsonData.hasKey(MatKeys::RasterizerState) ? JsonData[MatKeys::RasterizerState].ToString().c_str() : "";

	EBlendState BlendState = StringToBlendState(BlendStr, RenderPass);
	EDepthStencilState DepthState = StringToDepthStencilState(DepthStr, RenderPass);
	ERasterizerState RasterState = StringToRasterizerState(RasterStr, RenderPass);

	FString ShadowModeStr = JsonData.hasKey(MatKeys::ShadowMode) ? JsonData[MatKeys::ShadowMode].ToString().c_str() : "Opaque";
	EMaterialShadowMode ShadowMode = StringToShadowMode(ShadowModeStr);

	FVector4 EmissiveColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	float EmissiveIntensity = 0.0f;
	bool bEnableBloom = false;
	if (JsonData.hasKey(MatKeys::EmissiveColor))
	{
		EmissiveColor = ReadJsonVector4(JsonData[MatKeys::EmissiveColor], EmissiveColor);
	}
	if (JsonData.hasKey(MatKeys::EmissiveIntensity))
	{
		EmissiveIntensity = ReadJsonNumber(JsonData[MatKeys::EmissiveIntensity], EmissiveIntensity);
	}
	if (JsonData.hasKey(MatKeys::bEnableBloom))
	{
		bEnableBloom = ReadJsonBool(JsonData[MatKeys::bEnableBloom], bEnableBloom);
	}

	FMaterialTemplate* Template = GetOrCreateTemplate(ShaderPath);
	if (!Template)
	{
		return false;
	}

	UMaterial* Material = It->second;
	for (auto& Pair : Material->ConstantBufferMap)
	{
		if (Pair.second)
		{
			Pair.second->Release();
		}
	}
	Material->ConstantBufferMap.clear();
	Material->TextureParameters.clear();
	for (int32 Slot = 0; Slot < (int32)EMaterialTextureSlot::Max; ++Slot)
	{
		Material->CachedSRVs[Slot] = nullptr;
	}

	Material->PathFileName = PathFileName;
	Material->Template = Template;
	Material->TransientShader = nullptr;
	Material->RenderPass = RenderPass;
	Material->BlendState = BlendState;
	Material->DepthStencilState = DepthState;
	Material->RasterizerState = RasterState;
	Material->ShadowMode = ShadowMode;
	Material->EmissiveColor = EmissiveColor;
	Material->EmissiveIntensity = EmissiveIntensity;
	Material->bEnableBloom = bEnableBloom;
	Material->bMaterialBloomCBDirty = true;
	Material->ConstantBufferMap = CreateConstantBuffers(Template);

	InjectDefaultParameters(JsonData, Template, Material);
	PurgeStaleParameters(JsonData, Template);
	ApplyParameters(Material, JsonData);
	ApplyTextures(Material, JsonData);
	Material->RebuildCachedSRVs();

	return true;
}

bool FMaterialManager::ReloadMaterialInstance(const FString& MatInstFilePath)
{
	std::filesystem::path Path(FPaths::ToWide(MatInstFilePath));
	FString GenericPath = FPaths::ToUtf8(Path.generic_wstring());

	auto It = MaterialInstanceCache.find(GenericPath);
	if (It == MaterialInstanceCache.end() || !It->second)
	{
		return false;
	}

	json::JSON JsonData = ReadJsonFile(GenericPath);
	if (JsonData.IsNull())
	{
		return false;
	}

	FString ParentMaterialPath = JsonData[MatKeys::ParentMaterial].ToString().c_str();
	if (ParentMaterialPath.empty())
	{
		return false;
	}

	UMaterial* ParentMaterial = GetOrCreateMaterial(ParentMaterialPath);
	if (!ParentMaterial)
	{
		return false;
	}

	UMaterialInstance* MaterialInstance = It->second;
	for (auto& Pair : MaterialInstance->ConstantBufferMap)
	{
		if (Pair.second)
		{
			Pair.second->Release();
		}
	}
	MaterialInstance->ConstantBufferMap.clear();
	MaterialInstance->ScalarOverrides.clear();
	MaterialInstance->Vector3Overrides.clear();
	MaterialInstance->Vector4Overrides.clear();
	MaterialInstance->MatrixOverrides.clear();
	MaterialInstance->TextureOverrides.clear();
	for (int32 Slot = 0; Slot < (int32)EMaterialTextureSlot::Max; ++Slot)
	{
		MaterialInstance->CachedOverrideSRVs[Slot] = nullptr;
		MaterialInstance->bHasTextureOverride[Slot] = false;
	}

	MaterialInstance->PathFileName = GenericPath;
	MaterialInstance->Parent = ParentMaterial;
	MaterialInstance->ParentPathFileName = ParentMaterialPath;
	MaterialInstance->bOverrideEmissiveColor = false;
	MaterialInstance->bOverrideEmissiveIntensity = false;
	MaterialInstance->bOverrideBloomEnabled = false;
	MaterialInstance->EmissiveColorOverride = ParentMaterial->GetEmissiveColor();
	MaterialInstance->EmissiveIntensityOverride = ParentMaterial->GetEmissiveIntensity();
	MaterialInstance->bBloomEnabledOverride = ParentMaterial->IsBloomEnabled();
	MaterialInstance->bMaterialBloomCBDirty = true;
	MaterialInstance->bConstantBufferDirty = true;

	if (ParentMaterial->GetTemplate())
	{
		MaterialInstance->ConstantBufferMap = CreateConstantBuffers(ParentMaterial->GetTemplate());
	}
	MaterialInstance->CopyParentConstantBuffers();

	ApplyParameters(MaterialInstance, JsonData);
	ApplyTextures(MaterialInstance, JsonData);
	ApplyBloomOverrides(MaterialInstance, JsonData);

	return true;
}

bool FMaterialManager::ReloadMaterialInterface(const FString& AssetPath)
{
	if (AssetPath.empty() || AssetPath == "None")
	{
		return false;
	}

	std::filesystem::path Path(FPaths::ToWide(AssetPath));
	FString GenericPath = FPaths::ToUtf8(Path.generic_wstring());
	FString Extension = FPaths::ToUtf8(Path.extension().wstring());

	if (Extension == ".matinst")
	{
		return ReloadMaterialInstance(GenericPath);
	}

	return ReloadMaterial(GenericPath);
}

UMaterial* FMaterialManager::CreateTransientMaterial(ERenderPass InPass, EBlendState InBlend,
	EDepthStencilState InDepth, ERasterizerState InRaster, FShader* InShader)
{
	UMaterial* Material = UMaterial::CreateTransient(InPass, InBlend, InDepth, InRaster, InShader);
	if (!Material)
	{
		return nullptr;
	}

	const uint32 TransientId = NextTransientMaterialId++;
	TransientMaterialRegistry.emplace(TransientId, Material);
	return Material;
}

void FMaterialManager::DestroyTransientMaterial(UMaterial* Material)
{
	if (!Material)
	{
		return;
	}

	bool bFound = false;
	for (auto It = TransientMaterialRegistry.begin(); It != TransientMaterialRegistry.end(); ++It)
	{
		if (It->second == Material)
		{
			TransientMaterialRegistry.erase(It);
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		return;
	}

	Material->ReleaseGPUBuffers();
	UObjectManager::Get().DestroyObject(Material);
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

void FMaterialManager::ApplyParameters(UMaterialInterface* Material, json::JSON& JsonData)
{
	if (!Material || !JsonData.hasKey(MatKeys::Parameters)) return;

	for (auto& Pair : JsonData[MatKeys::Parameters].ObjectRange())
	{
		FString ParamName = Pair.first.c_str();
		json::JSON& Value = Pair.second;

		if (Value.JSONType() == json::JSON::Class::Array)
		{
			if (Value.length() == 3)
			{
				Material->SetVector3Parameter(ParamName, FVector((float)Value[0].ToFloat(), (float)Value[1].ToFloat(), (float)Value[2].ToFloat()));
			}
			else if (Value.length() == 4)
			{
				Material->SetVector4Parameter(ParamName, FVector4((float)Value[0].ToFloat(), (float)Value[1].ToFloat(), (float)Value[2].ToFloat(), (float)Value[3].ToFloat()));
			}
			else if (Value.length() == 16)
			{
				FMatrix MatrixValue;
				for (int32 i = 0; i < 16; ++i)
				{
					MatrixValue.Data[i] = (float)Value[i].ToFloat();
				}
				Material->SetMatrixParameter(ParamName, MatrixValue);
			}
		}
		else if (Value.JSONType() == json::JSON::Class::Floating || Value.JSONType() == json::JSON::Class::Integral)
		{
			Material->SetScalarParameter(ParamName, (float)Value.ToFloat());
		}
	}
}

void FMaterialManager::ApplyTextures(UMaterialInterface* Material, json::JSON& JsonData)
{
	if (!Material || !JsonData.hasKey(MatKeys::Textures)) return;

	for (auto& Pair : JsonData[MatKeys::Textures].ObjectRange())
	{
		FString SlotName = Pair.first.c_str();
		FString TexturePath = Pair.second.ToString().c_str();

		UTexture2D* Texture = nullptr;

		if (!TexturePath.empty() && TexturePath != "None")
		{
			const bool bIsColorTexture =
				SlotName == "DiffuseTexture" ||
				SlotName == "EmissiveTexture" ||
				SlotName == "Custom0Texture" ||
				SlotName == "Custom1Texture";

			Texture = UTexture2D::LoadFromFile(
				TexturePath,
				Device,
				bIsColorTexture ? ETextureColorSpace::SRGB : ETextureColorSpace::Linear);
		}

		Material->SetTextureParameter(SlotName, Texture);
	}
}

void FMaterialManager::ApplyBloomOverrides(UMaterialInstance* MaterialInstance, json::JSON& JsonData)
{
	if (!MaterialInstance)
	{
		return;
	}

	const bool bOverrideColor = JsonData.hasKey(MatKeys::bOverrideEmissiveColor)
		&& ReadJsonBool(JsonData[MatKeys::bOverrideEmissiveColor], false);
	const bool bOverrideIntensity = JsonData.hasKey(MatKeys::bOverrideEmissiveIntensity)
		&& ReadJsonBool(JsonData[MatKeys::bOverrideEmissiveIntensity], false);
	const bool bOverrideBloom = JsonData.hasKey(MatKeys::bOverrideEnableBloom)
		&& ReadJsonBool(JsonData[MatKeys::bOverrideEnableBloom], false);

	FVector4 EmissiveColor = MaterialInstance->GetEmissiveColor();
	if (JsonData.hasKey(MatKeys::EmissiveColor))
	{
		EmissiveColor = ReadJsonVector4(JsonData[MatKeys::EmissiveColor], EmissiveColor);
	}

	float EmissiveIntensity = MaterialInstance->GetEmissiveIntensity();
	if (JsonData.hasKey(MatKeys::EmissiveIntensity))
	{
		EmissiveIntensity = ReadJsonNumber(JsonData[MatKeys::EmissiveIntensity], EmissiveIntensity);
	}

	bool bEnableBloom = MaterialInstance->IsBloomEnabled();
	if (JsonData.hasKey(MatKeys::bEnableBloom))
	{
		bEnableBloom = ReadJsonBool(JsonData[MatKeys::bEnableBloom], bEnableBloom);
	}

	MaterialInstance->SetEmissiveColorOverride(bOverrideColor, EmissiveColor);
	MaterialInstance->SetEmissiveIntensityOverride(bOverrideIntensity, EmissiveIntensity);
	MaterialInstance->SetBloomEnabledOverride(bOverrideBloom, bEnableBloom);
}


ERenderPass FMaterialManager::StringToRenderPass(const FString& Str) const
{
	using namespace RenderStateStrings;
	return FromString(RenderPassMap, Str, ERenderPass::Opaque);
}

EBlendState FMaterialManager::StringToBlendState(const FString& Str, ERenderPass Pass) const
{
	using namespace RenderStateStrings;
	if (!Str.empty())
		return FromString(BlendStateMap, Str, EBlendState::Opaque);

	// 문자열이 비어있으면 Pass 기반 기본값
	switch (Pass)
	{
	case ERenderPass::AlphaBlend:
	case ERenderPass::Decal:
	case ERenderPass::EditorLines:
	case ERenderPass::PostProcess:
	case ERenderPass::GizmoInner:
	case ERenderPass::OverlayFont:
		return EBlendState::AlphaBlend;
	case ERenderPass::AdditiveDecal:
		return EBlendState::Additive;
	case ERenderPass::SelectionMask:
		return EBlendState::NoColor;
	default:
		return EBlendState::Opaque;
	}
}

EDepthStencilState FMaterialManager::StringToDepthStencilState(const FString& Str, ERenderPass Pass) const
{
	using namespace RenderStateStrings;
	if (!Str.empty())
		return FromString(DepthStencilStateMap, Str, EDepthStencilState::Default);

	// 문자열이 비어있으면 Pass 기반 기본값
	switch (Pass)
	{
	case ERenderPass::AlphaBlend:
	case ERenderPass::Decal:
	case ERenderPass::AdditiveDecal:
		return EDepthStencilState::DepthReadOnly;
	case ERenderPass::SelectionMask:
		return EDepthStencilState::StencilWrite;
	case ERenderPass::PostProcess:
	case ERenderPass::OverlayFont:
		return EDepthStencilState::NoDepth;
	case ERenderPass::GizmoOuter:
		return EDepthStencilState::GizmoOutside;
	case ERenderPass::GizmoInner:
		return EDepthStencilState::GizmoInside;
	default:
		return EDepthStencilState::Default;
	}
}

ERasterizerState FMaterialManager::StringToRasterizerState(const FString& Str, ERenderPass Pass) const
{
	using namespace RenderStateStrings;
	if (!Str.empty())
		return FromString(RasterizerStateMap, Str, ERasterizerState::SolidBackCull);

	// 문자열이 비어있으면 Pass 기반 기본값
	switch (Pass)
	{
	case ERenderPass::Decal:
	case ERenderPass::AdditiveDecal:
	case ERenderPass::SelectionMask:
	case ERenderPass::PostProcess:
		return ERasterizerState::SolidNoCull;
	default:
		return ERasterizerState::SolidBackCull;
	}
}

EMaterialShadowMode FMaterialManager::StringToShadowMode(const FString& Str) const
{
	const FEnum* EnumType = FEnum::FindEnumByName("EMaterialShadowMode");
	if (!EnumType || !EnumType->GetNames())
	{
		return EMaterialShadowMode::Opaque;
	}

	for (uint32 Index = 0; Index < EnumType->GetCount(); ++Index)
	{
		if (Str == EnumType->GetNames()[Index])
		{
			return static_cast<EMaterialShadowMode>(Index);
		}
	}

	return EMaterialShadowMode::Opaque;
}

const char* FMaterialManager::ShadowModeToString(EMaterialShadowMode Mode) const
{
	const FEnum* EnumType = FEnum::FindEnumByName("EMaterialShadowMode");
	if (!EnumType || !EnumType->GetNames())
	{
		return "Opaque";
	}

	uint32 Index = static_cast<uint32>(Mode);
	if (Index < EnumType->GetCount())
	{
		return EnumType->GetNames()[Index];
	}

	return "Opaque";
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
		if (!JsonData[MatKeys::Parameters][ParamName].IsNull())
			continue;

		bInjected = true;

		if (ParamName == "SectionColor")
		{
			JsonData[MatKeys::Parameters][ParamName] = json::Array(1.0f, 1.0f, 1.0f, 1.0f);
			continue;
		}

		if (ParamName == "HasNormalMap")
		{
			JsonData[MatKeys::Parameters][ParamName] = 0.0f;
			continue;
		}

		switch (Info->Size)
		{
			case sizeof(float) : // 4바이트 - Scalar
			{
				float Value = 0.f;
				Material->GetScalarParameter(ParamName, Value);
				JsonData[MatKeys::Parameters][ParamName] = Value;
				break;
			}
			case sizeof(float) * 3: // 12바이트 - Vector3
			{
				FVector Value;
				Material->GetVector3Parameter(ParamName, Value);
				JsonData[MatKeys::Parameters][ParamName] = json::Array(Value.X, Value.Y, Value.Z);
				break;
			}
			case sizeof(float) * 4: // 16바이트 - Vector4
			{
				FVector4 Value;
				Material->GetVector4Parameter(ParamName, Value);
				JsonData[MatKeys::Parameters][ParamName] = json::Array(Value.X, Value.Y, Value.Z, Value.W);
				break;
			}
			case sizeof(float) * 16: // 64바이트 - Matrix
			{
				FMatrix Value;
				Material->GetMatrixParameter(ParamName, Value);
				auto MatArray = json::Array();
				for (int i = 0; i < 16; ++i)
					MatArray.append(Value.Data[i]);
				JsonData[MatKeys::Parameters][ParamName] = MatArray;
				break;
			}
			default:
				break; // uint, bool 등 특수 케이스는 별도 처리 필요
		}
	}

	return bInjected;
}

bool FMaterialManager::PurgeStaleParameters(json::JSON& JsonData, FMaterialTemplate* Template)
{
	if (!JsonData.hasKey(MatKeys::Parameters)) return false;

	const auto& Layout = Template->GetParameterInfo();
	json::JSON CleanParams = json::JSON::Make(json::JSON::Class::Object);
	bool bPurged = false;

	for (auto& Pair : JsonData[MatKeys::Parameters].ObjectRange())
	{
		FString ParamName = Pair.first.c_str();
		if (Layout.find(ParamName) != Layout.end())
		{
			CleanParams[Pair.first] = Pair.second;
		}
		else
		{
			bPurged = true;
		}
	}

	if (bPurged)
	{
		JsonData[MatKeys::Parameters] = std::move(CleanParams);
	}

	return bPurged;
}

FMaterialTemplate* FMaterialManager::GetOrCreateTemplate(const FString& ShaderPath)
{
	// 1. 템플릿이 캐시에 있는지 확인 (셰이더 경로를 키값으로 사용)
	auto It = TemplateCache.find(ShaderPath);
	if (It != TemplateCache.end())
	{
		return It->second;
	}

	// 2. 템플릿이 기존에 없다면 새로 제작
	//    캐시에 있으면 반환, 없으면 컴파일 후 캐싱
	FShader* Shader = FShaderManager::Get().FindOrCreate(ShaderPath);
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
	if (!Device)
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

	// 2. GPU 버퍼를 Device 해제 전에 명시 해제, UObject 수명은 UObjectManager가 관리
	for (auto& [Key, Mat] : MaterialCache)
	{
		if (Mat) Mat->ReleaseGPUBuffers();
	}
	MaterialCache.clear();

	for (auto& [Key, Mat] : TransientMaterialRegistry)
	{
		if (Mat) Mat->ReleaseGPUBuffers();
	}
	TransientMaterialRegistry.clear();

	for (auto& [Key, MatInst] : MaterialInstanceCache)
	{
		if (MatInst) MatInst->ReleaseGPUBuffers();
	}
	MaterialInstanceCache.clear();

	// 3. Device 참조 해제
	// 외부에서 주입받은 리소스이므로 포인터만 초기화합니다.
	Device = nullptr;
}

void FMaterialManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto& [Path, Mat] : MaterialCache)
	{
		Collector.AddReferencedObject(Mat);
	}

	for (auto& [Path, MatInst] : MaterialInstanceCache)
	{
		Collector.AddReferencedObject(MatInst);
	}

	for (auto& [Key, Mat] : TransientMaterialRegistry)
	{
		Collector.AddReferencedObject(Mat);
	}
}
