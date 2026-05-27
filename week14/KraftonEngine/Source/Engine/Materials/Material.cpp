#include "Materials/Material.h"
#include "Serialization/Archive.h"
#include "Texture/Texture2D.h"
#include "Engine/Runtime/Engine.h"
#include "Render/Pipeline/Renderer.h"
#include "Render/Types/MaterialTextureSlot.h"

// ─── UMaterial ───

UMaterial::~UMaterial()
{
	for (auto& Pair : ConstantBufferMap)
	{
		Pair.second->Release();
	}
	ConstantBufferMap.clear();
	MaterialBloomCB.Release();

	for (auto& Pair : TextureParameters)
	{
		Pair.second = nullptr;
	}
}

void UMaterial::Create(const FString& InPathFileName, FMaterialTemplate* InTemplate,
	ERenderPass InRenderPass,
	EBlendState InBlend,
	EDepthStencilState InDepth,
	ERasterizerState InRaster,
	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>>&& InBuffers)
{
	PathFileName = InPathFileName;
	Template = InTemplate;
	RenderPass = InRenderPass;
	BlendState = InBlend;
	DepthStencilState = InDepth;
	RasterizerState = InRaster;

	ConstantBufferMap = std::move(InBuffers);
}

const TMap<FString, FMaterialParameterInfo*>& UMaterial::GetParameterInfo() const
{
	static const TMap<FString, FMaterialParameterInfo*> EmptyLayout;
	return Template ? Template->GetParameterInfo() : EmptyLayout;
}

const uint8* UMaterial::GetRawPtr(const FString& BufferName, uint32 Offset) const
{
	auto It = ConstantBufferMap.find(BufferName);
	if (It == ConstantBufferMap.end() || !It->second || !It->second->CPUData || Offset >= It->second->Size)
	{
		return nullptr;
	}

	return It->second->CPUData + Offset;
}

TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> UMaterial::CloneConstantBuffers(ID3D11Device* Device) const
{
	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> ClonedBuffers;

	for (const auto& Pair : ConstantBufferMap)
	{
		if (!Pair.second)
		{
			continue;
		}

		auto NewBuffer = std::make_unique<FMaterialConstantBuffer>();
		NewBuffer->Init(Device, Pair.second->Size, Pair.second->SlotIndex);
		if (Pair.second->CPUData && NewBuffer->CPUData)
		{
			memcpy(NewBuffer->CPUData, Pair.second->CPUData, Pair.second->Size);
		}
		NewBuffer->bDirty = true;
		ClonedBuffers.emplace(Pair.first, std::move(NewBuffer));
	}

	return ClonedBuffers;
}

void UMaterial::CopyTextureParametersFrom(const UMaterial& Other)
{
	TextureParameters = Other.TextureParameters;
	RebuildCachedSRVs();
}

bool UMaterial::SetParameter(const FString& Name, const void* Data, uint32 Size)
{
	if (!Template || !Data)
	{
		return false;
	}

	FMaterialParameterInfo Info;
	if (!Template->GetParameterInfo(Name, Info)) {
		return false;
	}
	auto It = ConstantBufferMap.find(Info.BufferName);
	if (It == ConstantBufferMap.end()) return false;

	It->second->SetData(Data, Size, Info.Offset);
	It->second->bDirty = true;

	if (GEngine)
	{
		It->second->Upload(GEngine->GetRenderer().GetFD3DDevice().GetDeviceContext());
	}
	return true;
}


bool UMaterial::SetScalarParameter(const FString& ParamName, float Value)
{
	return SetParameter(ParamName, &Value, sizeof(float));
}

bool UMaterial::SetVector3Parameter(const FString& ParamName, const FVector& Value)
{
	float Data[3] = { Value.X, Value.Y, Value.Z };
	return SetParameter(ParamName, Data, sizeof(Data));
}

bool UMaterial::SetVector4Parameter(const FString& ParamName, const FVector4& Value)
{
	float Data[4] = { Value.X, Value.Y, Value.Z, Value.W };
	return SetParameter(ParamName, Data, sizeof(Data));
}

bool UMaterial::SetTextureParameter(const FString& ParamName, UTexture2D* Texture)
{
	TextureParameters[ParamName] = Texture;

	// CachedSRVs 갱신 — 슬롯 이름과 매칭되면 즉시 반영
	for (int s = 0; s < (int)EMaterialTextureSlot::Max; s++)
	{
		FString SlotName = MaterialTextureSlot::ToString(s) + "Texture";
		if (ParamName == SlotName)
		{
			CachedSRVs[s] = (Texture && Texture->GetSRV()) ? Texture->GetSRV() : nullptr;
			break;
		}
	}

	return true;
}

bool UMaterial::SetMatrixParameter(const FString& ParamName, const FMatrix& Value)
{
	return SetParameter(ParamName, Value.Data, sizeof(float) * 16);
}

bool UMaterial::GetScalarParameter(const FString& ParamName, float& OutValue) const
{
	if (!Template)
	{
		return false;
	}

	FMaterialParameterInfo Info;
	if (!Template->GetParameterInfo(ParamName, Info)) return false;

	auto It = ConstantBufferMap.find(Info.BufferName);
	if (It == ConstantBufferMap.end()) return false;

	const uint8* Ptr = It->second->CPUData + Info.Offset;
	OutValue = *reinterpret_cast<const float*>(Ptr);
	return true;
}

bool UMaterial::GetVector3Parameter(const FString& ParamName, FVector& OutValue) const
{
	if (!Template)
	{
		return false;
	}

	FMaterialParameterInfo Info;
	if (!Template->GetParameterInfo(ParamName, Info)) return false;

	auto It = ConstantBufferMap.find(Info.BufferName);
	if (It == ConstantBufferMap.end()) return false;

	const uint8* Ptr = It->second->CPUData + Info.Offset;
	OutValue = *reinterpret_cast<const FVector*>(Ptr);
	return true;
}

bool UMaterial::GetVector4Parameter(const FString& ParamName, FVector4& OutValue) const
{
	if (!Template)
	{
		return false;
	}

	FMaterialParameterInfo Info;
	if (!Template->GetParameterInfo(ParamName, Info)) return false;

	auto It = ConstantBufferMap.find(Info.BufferName);
	if (It == ConstantBufferMap.end()) return false;

	const uint8* Ptr = It->second->CPUData + Info.Offset;
	OutValue = *reinterpret_cast<const FVector4*>(Ptr);
	return true;
}

bool UMaterial::GetTextureParameter(const FString& ParamName, UTexture2D*& OutTexture) const
{
	auto It = TextureParameters.find(ParamName);
	if (It == TextureParameters.end()) return false;

	OutTexture = It->second;
	return true;
}

bool UMaterial::GetMatrixParameter(const FString& ParamName, FMatrix& Value) const
{
	if (!Template)
	{
		return false;
	}

	FMaterialParameterInfo Info;
	if (!Template->GetParameterInfo(ParamName, Info)) return false;

	auto It = ConstantBufferMap.find(Info.BufferName);
	if (It == ConstantBufferMap.end()) return false;

	const uint8* Ptr = It->second->CPUData + Info.Offset;
	memcpy(Value.Data, Ptr, sizeof(float) * 16);
	return true;
}

void UMaterial::Bind(ID3D11DeviceContext* Context)
{
}

void UMaterial::PostEditProperty(const char* PropertyName)
{
	UObject::PostEditProperty(PropertyName);
	bMaterialBloomCBDirty = true;
}

const FString& UMaterial::GetTexturePathFileName(const FString& TextureName)const
{
	auto it = TextureParameters.find(TextureName);
	if (it != TextureParameters.end())
	{
		UTexture2D* Texture = it->second;
		if(Texture)
		{
			return Texture->GetSourcePath();
		}
	}
	static const FString EmptyString;
	return EmptyString;
}

void UMaterial::RebuildCachedSRVs()
{
	for (int s = 0; s < (int)EMaterialTextureSlot::Max; s++)
	{
		CachedSRVs[s] = nullptr;
		UTexture2D* Tex = nullptr;
		FString SlotName = MaterialTextureSlot::ToString(s) + "Texture";
		if (GetTextureParameter(SlotName, Tex) && Tex && Tex->GetSRV())
			CachedSRVs[s] = Tex->GetSRV();
	}
}

void UMaterial::Serialize(FArchive& Ar)
{
	Ar << PathFileName;

	uint32 BufferCount = static_cast<uint32>(ConstantBufferMap.size());
	Ar << BufferCount;

	if (Ar.IsSaving())
	{
		for (auto& Pair : ConstantBufferMap)
		{
			FString BufferName = Pair.first;
			uint32 Size = Pair.second->Size;

			Ar << BufferName;
			Ar << Size;
			Ar.Serialize(Pair.second->CPUData, Size);
		}
	}

	if (Ar.IsLoading())
	{
		for (uint32 i = 0; i < BufferCount; ++i)
		{
			FString BufferName;
			uint32 Size = 0;

			Ar << BufferName;
			Ar << Size;

			auto It = ConstantBufferMap.find(BufferName);
			if (It != ConstantBufferMap.end())
			{
				Ar.Serialize(It->second->CPUData, Size);
				It->second->bDirty = true;
				It->second->Upload(GEngine->GetRenderer().GetFD3DDevice().GetDeviceContext());
			}
			else
			{
				TArray<uint8> Dummy(Size);
				Ar.Serialize(Dummy.data(), Size);
			}
		}
	}
	
	uint32 TextureCount = static_cast<uint32>(TextureParameters.size());
	Ar << TextureCount;

	if (Ar.IsSaving())
	{
		for (auto& Pair : TextureParameters)
		{
			FString SlotName = Pair.first;
			FString TexturePath = Pair.second ? Pair.second->GetSourcePath() : FString();

			Ar << SlotName;
			Ar << TexturePath;
		}
	}
	else // IsLoading
	{
		for (uint32 i = 0; i < TextureCount; ++i)
		{
			FString SlotName;
			FString TexturePath;

			Ar << SlotName;
			Ar << TexturePath;

			if (!TexturePath.empty())
			{
				ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
				const bool bIsColorTexture =
					SlotName == "DiffuseTexture" ||
					SlotName == "EmissiveTexture" ||
					SlotName == "Custom0Texture" ||
					SlotName == "Custom1Texture";
				UTexture2D* Loaded = UTexture2D::LoadFromFile(
					TexturePath,
					Device,
					bIsColorTexture ? ETextureColorSpace::SRGB : ETextureColorSpace::Linear);
				if (Loaded)
				{
					TextureParameters[SlotName] = Loaded;
				}
			}
		}

		RebuildCachedSRVs();
	}
}

UMaterial* UMaterial::CreateTransient(ERenderPass InPass, EBlendState InBlend,
	EDepthStencilState InDepth, ERasterizerState InRaster, FShader* InShader)
{
	UMaterial* Mat = UObjectManager::Get().CreateObject<UMaterial>();
	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> EmptyBuffers;
	Mat->Create(FString("__transient__"), nullptr, InPass, InBlend, InDepth, InRaster, std::move(EmptyBuffers));
	Mat->TransientShader = InShader;


	return Mat;
}

UMaterialInstance::~UMaterialInstance()
{
	ReleaseGPUBuffers();
}

UMaterialInstance* UMaterialInstance::Create(UMaterial* InParent)
{
	if (!InParent)
	{
		return nullptr;
	}

	UMaterialInstance* Instance = UObjectManager::Get().CreateObject<UMaterialInstance>();
	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> EmptyBuffers;
	Instance->Create(InParent->GetAssetPathFileName(), InParent, std::move(EmptyBuffers));

	return Instance;
}

UMaterialInstance* UMaterialInstance::Create(UMaterial* InParent,
	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>>&& InBuffers)
{
	if (!InParent)
	{
		return nullptr;
	}

	UMaterialInstance* Instance = UObjectManager::Get().CreateObject<UMaterialInstance>();
	Instance->Create(InParent->GetAssetPathFileName(), InParent, std::move(InBuffers));
	return Instance;
}

void UMaterialInstance::Create(const FString& InPathFileName, UMaterial* InParent,
	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>>&& InBuffers)
{
	PathFileName = InPathFileName;
	Parent = InParent;
	ParentPathFileName = Parent ? Parent->GetAssetPathFileName() : FString();
	ConstantBufferMap = std::move(InBuffers);
	CopyParentConstantBuffers();
	bConstantBufferDirty = true;
	EmissiveColorOverride = Parent ? Parent->GetEmissiveColor() : FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	EmissiveIntensityOverride = Parent ? Parent->GetEmissiveIntensity() : 0.0f;
	bBloomEnabledOverride = Parent ? Parent->IsBloomEnabled() : false;
	bMaterialBloomCBDirty = true;
}

void UMaterialInstance::Serialize(FArchive& Ar)
{
	Ar << ParentPathFileName;
	//Ar << ScalarOverrides;
	//Ar << Vector3Overrides;
	//Ar << Vector4Overrides;
	//Ar << MatrixOverrides;
	//Ar << TextureOverrides;
}

void UMaterialInstance::CopyParentConstantBuffers()
{
	if (!Parent)
	{
		return;
	}

	for (auto& Pair : ConstantBufferMap)
	{
		if (!Pair.second || !Pair.second->CPUData)
		{
			continue;
		}

		const uint8* ParentData = Parent->GetRawPtr(Pair.first, 0);
		if (!ParentData)
		{
			continue;
		}

		memcpy(Pair.second->CPUData, ParentData, Pair.second->Size);
		Pair.second->bDirty = true;
	}
}

bool UMaterialInstance::SetParameter(const FString& Name, const void* Data, uint32 Size)
{
	if (!Parent || !Data)
	{
		return false;
	}

	FMaterialTemplate* Template = Parent->GetTemplate();
	if (!Template)
	{
		return false;
	}

	FMaterialParameterInfo Info;
	if (!Template->GetParameterInfo(Name, Info))
	{
		return false;
	}

	auto It = ConstantBufferMap.find(Info.BufferName);
	if (It == ConstantBufferMap.end() || !It->second)
	{
		return false;
	}

	It->second->SetData(Data, Size, Info.Offset);
	bConstantBufferDirty = true;
	return true;
}

bool UMaterialInstance::SetScalarParameter(const FString& ParamName, float Value)
{
	float ParentValue = 0.0f;
	if (!Parent || !Parent->GetScalarParameter(ParamName, ParentValue))
	{
		return false;
	}

	ScalarOverrides[ParamName] = Value;
	return SetParameter(ParamName, &Value, sizeof(float));
}

bool UMaterialInstance::SetVector3Parameter(const FString& ParamName, const FVector& Value)
{
	FVector ParentValue;
	if (!Parent || !Parent->GetVector3Parameter(ParamName, ParentValue))
	{
		return false;
	}

	Vector3Overrides[ParamName] = Value;
	float Data[3] = { Value.X, Value.Y, Value.Z };
	return SetParameter(ParamName, Data, sizeof(Data));
}

bool UMaterialInstance::SetVector4Parameter(const FString& ParamName, const FVector4& Value)
{
	FVector4 ParentValue;
	if (!Parent || !Parent->GetVector4Parameter(ParamName, ParentValue))
	{
		return false;
	}

	Vector4Overrides[ParamName] = Value;
	float Data[4] = { Value.X, Value.Y, Value.Z, Value.W };
	return SetParameter(ParamName, Data, sizeof(Data));
}

bool UMaterialInstance::SetTextureParameter(const FString& ParamName, UTexture2D* Texture)
{
	UTexture2D* ParentTexture = nullptr;
	if (!Parent || !Parent->GetTextureParameter(ParamName, ParentTexture))
	{
		return false;
	}
	int SlotIndex = (int)MaterialTextureSlot::FromParameterName(ParamName);

	bHasTextureOverride[SlotIndex] = true;
	CachedOverrideSRVs[SlotIndex] = Texture ? Texture->GetSRV() : nullptr;

	TextureOverrides[ParamName] = Texture;
	bConstantBufferDirty = true;
	return true;
}

bool UMaterialInstance::SetMatrixParameter(const FString& ParamName, const FMatrix& Value)
{
	FMatrix ParentValue;
	if (!Parent || !Parent->GetMatrixParameter(ParamName, ParentValue))
	{
		return false;
	}

	MatrixOverrides[ParamName] = Value;
	return SetParameter(ParamName, Value.Data, sizeof(float) * 16);
}

bool UMaterialInstance::GetScalarParameter(const FString& ParamName, float& OutValue) const
{
	auto It = ScalarOverrides.find(ParamName);
	if (It != ScalarOverrides.end())
	{
		OutValue = It->second;
		return true;
	}

	return Parent ? Parent->GetScalarParameter(ParamName, OutValue) : false;
}

bool UMaterialInstance::GetVector3Parameter(const FString& ParamName, FVector& OutValue) const
{
	auto It = Vector3Overrides.find(ParamName);
	if (It != Vector3Overrides.end())
	{
		OutValue = It->second;
		return true;
	}

	return Parent ? Parent->GetVector3Parameter(ParamName, OutValue) : false;
}

bool UMaterialInstance::GetVector4Parameter(const FString& ParamName, FVector4& OutValue) const
{
	auto It = Vector4Overrides.find(ParamName);
	if (It != Vector4Overrides.end())
	{
		OutValue = It->second;
		return true;
	}

	return Parent ? Parent->GetVector4Parameter(ParamName, OutValue) : false;
}

bool UMaterialInstance::GetTextureParameter(const FString& ParamName, UTexture2D*& OutTexture) const
{
	auto It = TextureOverrides.find(ParamName);
	if (It != TextureOverrides.end())
	{
		OutTexture = It->second;
		return true;
	}

	return Parent ? Parent->GetTextureParameter(ParamName, OutTexture) : false;
}

bool UMaterialInstance::GetMatrixParameter(const FString& ParamName, FMatrix& Value) const
{
	auto It = MatrixOverrides.find(ParamName);
	if (It != MatrixOverrides.end())
	{
		Value = It->second;
		return true;
	}

	return Parent ? Parent->GetMatrixParameter(ParamName, Value) : false;
}

FVector4 UMaterialInstance::GetEmissiveColor() const
{
	return bOverrideEmissiveColor
		? EmissiveColorOverride
		: (Parent ? Parent->GetEmissiveColor() : FVector4(1.0f, 1.0f, 1.0f, 1.0f));
}

float UMaterialInstance::GetEmissiveIntensity() const
{
	return bOverrideEmissiveIntensity
		? EmissiveIntensityOverride
		: (Parent ? Parent->GetEmissiveIntensity() : 0.0f);
}

bool UMaterialInstance::IsBloomEnabled() const
{
	return bOverrideBloomEnabled
		? bBloomEnabledOverride
		: (Parent ? Parent->IsBloomEnabled() : false);
}

void UMaterialInstance::SetEmissiveColorOverride(bool bOverride, const FVector4& InColor)
{
	bOverrideEmissiveColor = bOverride;
	EmissiveColorOverride = InColor;
	bMaterialBloomCBDirty = true;
}

void UMaterialInstance::SetEmissiveIntensityOverride(bool bOverride, float InIntensity)
{
	bOverrideEmissiveIntensity = bOverride;
	EmissiveIntensityOverride = InIntensity;
	bMaterialBloomCBDirty = true;
}

void UMaterialInstance::SetBloomEnabledOverride(bool bOverride, bool bInEnableBloom)
{
	bOverrideBloomEnabled = bOverride;
	bBloomEnabledOverride = bInEnableBloom;
	bMaterialBloomCBDirty = true;
}

void UMaterialInstance::ReleaseGPUBuffers()
{
	for (auto& Pair : ConstantBufferMap)
	{
		if (Pair.second)
		{
			Pair.second->Release();
		}
	}
	MaterialBloomCB.Release();
	bMaterialBloomCBDirty = true;
}

FShader* UMaterialInstance::GetShader() const
{
	return Parent ? Parent->GetShader() : nullptr;
}

ERenderPass UMaterialInstance::GetRenderPass() const
{
	return Parent ? Parent->GetRenderPass() : ERenderPass::Opaque;
}

EBlendState UMaterialInstance::GetBlendState() const
{
	return Parent ? Parent->GetBlendState() : EBlendState::Opaque;
}

EDepthStencilState UMaterialInstance::GetDepthStencilState() const
{
	return Parent ? Parent->GetDepthStencilState() : EDepthStencilState::Default;
}

ERasterizerState UMaterialInstance::GetRasterizerState() const
{
	return Parent ? Parent->GetRasterizerState() : ERasterizerState::SolidBackCull;
}

FConstantBuffer* UMaterialInstance::GetGPUBufferBySlot(uint32 InSlot) const
{
	if (InSlot == ECBSlot::MaterialBloom)
	{
		return &MaterialBloomCB;
	}

	for (const auto& Pair : ConstantBufferMap)
	{
		if (Pair.second && Pair.second->SlotIndex == InSlot)
		{
			return Pair.second->GetConstantBuffer();
		}
	}

	return nullptr;
}

ID3D11ShaderResourceView* UMaterialInstance::GetSRV(EMaterialTextureSlot Slot) const
{
	//1. cached override SRV가 있으면 반환
	int Index = (int)Slot;
	if (bHasTextureOverride[Index])
		return CachedOverrideSRVs[Index];

	//2. cached가 없으면 override map에서 조회
	FString SlotName = MaterialTextureSlot::ToString((int)Slot) + "Texture";
	auto It = TextureOverrides.find(SlotName);
	if (It != TextureOverrides.end())
	{
		return (It->second && It->second->GetSRV()) ? It->second->GetSRV() : nullptr;
	}

	//3. override가 없으면 부모에서 조회
	return Parent ? Parent->GetSRV(Slot) : nullptr;
}

void UMaterialInstance::FlushDirtyBuffers(ID3D11Device* Device, ID3D11DeviceContext* Ctx)
{
	if (!Parent)
	{
		return;
	}

	Parent->FlushDirtyBuffers(Device, Ctx);

	if (bConstantBufferDirty)
	{
		CopyParentConstantBuffers();

		for (const auto& Pair : ScalarOverrides)
		{
			SetParameter(Pair.first, &Pair.second, sizeof(float));
		}
		for (const auto& Pair : Vector3Overrides)
		{
			float Data[3] = { Pair.second.X, Pair.second.Y, Pair.second.Z };
			SetParameter(Pair.first, Data, sizeof(Data));
		}
		for (const auto& Pair : Vector4Overrides)
		{
			float Data[4] = { Pair.second.X, Pair.second.Y, Pair.second.Z, Pair.second.W };
			SetParameter(Pair.first, Data, sizeof(Data));
		}
		for (const auto& Pair : MatrixOverrides)
		{
			SetParameter(Pair.first, Pair.second.Data, sizeof(float) * 16);
		}
	}

	for (auto& Pair : ConstantBufferMap)
	{
		if (Pair.second && Pair.second->bDirty)
		{
			Pair.second->Upload(Ctx);
		}
	}

	const bool bNeedsCreate = MaterialBloomCB.GetBuffer() == nullptr;
	if (bNeedsCreate)
	{
		MaterialBloomCB.Create(Device, sizeof(FMaterialBloomConstants), "MaterialInstanceBloomCB");
	}
	if (bNeedsCreate || bMaterialBloomCBDirty)
	{
		FMaterialBloomConstants BloomData = {};
		BloomData.EmissiveColor = GetEmissiveColor();
		BloomData.EmissiveIntensity = GetEmissiveIntensity();
		BloomData.bEnableBloom = IsBloomEnabled() ? 1.0f : 0.0f;
		MaterialBloomCB.Update(Ctx, &BloomData, sizeof(FMaterialBloomConstants));
		bMaterialBloomCBDirty = false;
	}

	bConstantBufferDirty = false;
}

UMaterialInstanceDynamic* UMaterialInstanceDynamic::Create(UMaterial* InParent)
{
	if (!InParent)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* Instance = UObjectManager::Get().CreateObject<UMaterialInstanceDynamic>();
	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> EmptyBuffers;
	Instance->UMaterialInstance::Create(InParent->GetAssetPathFileName(), InParent, std::move(EmptyBuffers));

	return Instance;
}

UMaterialInstanceDynamic* UMaterialInstanceDynamic::Create(UMaterial* InParent,
	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>>&& InBuffers)
{
	if (!InParent)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* Instance = UObjectManager::Get().CreateObject<UMaterialInstanceDynamic>();
	Instance->UMaterialInstance::Create(InParent->GetAssetPathFileName(), InParent, std::move(InBuffers));
	return Instance;
}
