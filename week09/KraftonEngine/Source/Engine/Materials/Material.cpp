#include "Render/Types/PipelineStateTypes.h"
#include "Render/Pipelines/RenderPassTypes.h"
#include "Materials/Material.h"
#include "Serialization/Archive.h"
#include "Render/RHI/D3D11/Shaders/GraphicsShaderProgram.h"
#include "Texture/Texture2D.h"
#include "Engine/Runtime/Engine.h"
#include "Render/Renderer.h"


IMPLEMENT_CLASS(UMaterial, UMaterialInterface)

// ─── UMaterial ───

UMaterial::~UMaterial()
{
    for (auto& Pair : ConstantBufferMap)
    {
        Pair.second->Release();
    }
    ConstantBufferMap.clear();

    for (auto& Pair : TextureParameters)
    {
        Pair.second = nullptr;
    }

    LooseScalarParameters.clear();
    LooseVector3Parameters.clear();
    LooseVector4Parameters.clear();
    LooseMatrixParameters.clear();
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

bool UMaterial::SetParameter(const FString& Name, const void* Data, uint32 Size)
{
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
    if (It == ConstantBufferMap.end())
        return false;

    It->second->SetData(Data, Size, Info.Offset);
    It->second->bDirty = true;

    It->second->Upload(GEngine->GetRenderer().GetFD3DDevice().GetDeviceContext());
    return true;
}


bool UMaterial::SetScalarParameter(const FString& ParamName, float Value)
{
    LooseScalarParameters[ParamName] = Value;
    SetParameter(ParamName, &Value, sizeof(float));
    return true;
}

bool UMaterial::SetVector3Parameter(const FString& ParamName, const FVector& Value)
{
    LooseVector3Parameters[ParamName] = Value;
    float Data[3] = { Value.X, Value.Y, Value.Z };
    SetParameter(ParamName, Data, sizeof(Data));
    return true;
}

bool UMaterial::SetVector4Parameter(const FString& ParamName, const FVector4& Value)
{
    LooseVector4Parameters[ParamName] = Value;
    float Data[4] = { Value.X, Value.Y, Value.Z, Value.W };
    SetParameter(ParamName, Data, sizeof(Data));
    return true;
}

bool UMaterial::SetTextureParameter(const FString& ParamName, UTexture2D* Texture)
{
    TextureParameters[ParamName] = Texture;
    return true;
}

bool UMaterial::SetMatrixParameter(const FString& ParamName, const FMatrix& Value)
{
    LooseMatrixParameters[ParamName] = Value;
    SetParameter(ParamName, Value.Data, sizeof(float) * 16);
    return true;
}

bool UMaterial::GetScalarParameter(const FString& ParamName, float& OutValue) const
{
    if (Template)
    {
        FMaterialParameterInfo Info;
        if (Template->GetParameterInfo(ParamName, Info))
        {
            auto It = ConstantBufferMap.find(Info.BufferName);
            if (It != ConstantBufferMap.end())
            {
                const uint8* Ptr = It->second->CPUData + Info.Offset;
                OutValue = *reinterpret_cast<const float*>(Ptr);
                return true;
            }
        }
    }

    auto LooseIt = LooseScalarParameters.find(ParamName);
    if (LooseIt == LooseScalarParameters.end())
        return false;

    OutValue = LooseIt->second;
    return true;
}

bool UMaterial::GetVector3Parameter(const FString& ParamName, FVector& OutValue) const
{
    if (Template)
    {
        FMaterialParameterInfo Info;
        if (Template->GetParameterInfo(ParamName, Info))
        {
            auto It = ConstantBufferMap.find(Info.BufferName);
            if (It != ConstantBufferMap.end())
            {
                const uint8* Ptr = It->second->CPUData + Info.Offset;
                OutValue = *reinterpret_cast<const FVector*>(Ptr);
                return true;
            }
        }
    }

    auto LooseIt = LooseVector3Parameters.find(ParamName);
    if (LooseIt == LooseVector3Parameters.end())
        return false;

    OutValue = LooseIt->second;
    return true;
}

bool UMaterial::GetVector4Parameter(const FString& ParamName, FVector4& OutValue) const
{
    if (Template)
    {
        FMaterialParameterInfo Info;
        if (Template->GetParameterInfo(ParamName, Info))
        {
            auto It = ConstantBufferMap.find(Info.BufferName);
            if (It != ConstantBufferMap.end())
            {
                const uint8* Ptr = It->second->CPUData + Info.Offset;
                OutValue = *reinterpret_cast<const FVector4*>(Ptr);
                return true;
            }
        }
    }

    auto LooseIt = LooseVector4Parameters.find(ParamName);
    if (LooseIt == LooseVector4Parameters.end())
        return false;

    OutValue = LooseIt->second;
    return true;
}

bool UMaterial::GetTextureParameter(const FString& ParamName, UTexture2D*& OutTexture) const
{
    auto It = TextureParameters.find(ParamName);
    if (It == TextureParameters.end())
        return false;

    OutTexture = It->second;
    return true;
}

bool UMaterial::GetMatrixParameter(const FString& ParamName, FMatrix& Value) const
{
    if (Template)
    {
        FMaterialParameterInfo Info;
        if (Template->GetParameterInfo(ParamName, Info))
        {
            auto It = ConstantBufferMap.find(Info.BufferName);
            if (It != ConstantBufferMap.end())
            {
                const uint8* Ptr = It->second->CPUData + Info.Offset;
                memcpy(Value.Data, Ptr, sizeof(float) * 16);
                return true;
            }
        }
    }

    auto LooseIt = LooseMatrixParameters.find(ParamName);
    if (LooseIt == LooseMatrixParameters.end())
        return false;

    Value = LooseIt->second;
    return true;
}


const FString& UMaterial::GetTexturePathFileName(const FString& TextureName) const
{
    auto it = TextureParameters.find(TextureName);
    if (it != TextureParameters.end())
    {
        UTexture2D* Texture = it->second;
        if (Texture)
        {
            return Texture->GetSourcePath();
        }
    }
    static const FString EmptyString;
    return EmptyString;
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
                TextureParameters[SlotName] = UTexture2D::LoadFromFile(TexturePath, Device);
            }
        }
    }
}

// ─── UMaterialInstanceDynamic ───

IMPLEMENT_CLASS(UMaterialInstanceDynamic, UMaterial)

UMaterialInstanceDynamic* UMaterialInstanceDynamic::Create(UMaterial* InParent)
{
    return nullptr;
}

void UMaterialInstanceDynamic::Serialize(FArchive& Ar)
{
    UMaterial::Serialize(Ar);
    Ar << ParentPathFileName;
}
