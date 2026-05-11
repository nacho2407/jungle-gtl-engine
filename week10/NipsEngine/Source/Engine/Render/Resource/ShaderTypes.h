#pragma once

#include "Core/CoreTypes.h"
#include "Core/Containers/Array.h"
#include "Core/Containers/Map.h"
#include "Core/Containers/String.h"
#include "Render/Common/RenderTypes.h"

#include <functional>

// ShaderStage 하나를 구분하는 키입니다.
// 같은 파일이라도 EntryPoint / Target / PermutationKey가 다르면 별도 컴파일 결과로 취급합니다.
struct FShaderStageKey
{
    FString FilePath;
    FString EntryPoint;
    FString Target;
    uint32 PermutationKey = 0;

    bool operator==(const FShaderStageKey& Other) const
    {
        return FilePath == Other.FilePath
            && EntryPoint == Other.EntryPoint
            && Target == Other.Target
            && PermutationKey == Other.PermutationKey;
    }
};

// ShaderStageKey를 unordered_map 키로 쓰기 위한 해시입니다.
struct FShaderStageKeyHasher
{
    size_t operator()(const FShaderStageKey& Key) const
    {
        size_t Hash = std::hash<FString>{}(Key.FilePath);
        Hash ^= std::hash<FString>{}(Key.EntryPoint) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
        Hash ^= std::hash<FString>{}(Key.Target) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
        Hash ^= std::hash<uint32>{}(Key.PermutationKey) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
        return Hash;
    }
};

// PixelShader Reflection에서 얻은 상수 버퍼 변수 위치 정보입니다.
// Material Parameter를 이름 기반으로 찾아서 해당 offset에 써 넣기 위해 사용합니다.
struct FShaderVariableReflection
{
    uint32 BufferSlot = 0;
    uint32 Offset = 0;
    uint32 Size = 0;
};

// Shader Reflection 결과입니다.
// 현재는 Material Parameter 바인딩에 필요한 Texture Slot / CBuffer Variable 정보를 중심으로 들고 있습니다.
struct FShaderReflectionInfo
{
    TMap<FString, uint32> TextureBindSlots;
    TMap<FString, FShaderVariableReflection> Variables;
    uint32 ConstantBufferSize = 0;
};

// 명시적 VertexLayout을 열어두기 위한 구조입니다.
// 현재 InputLayout은 VS Reflection 기반으로 생성하지만, 이후 수동 Layout이 필요해질 때 사용합니다.
struct FVertexElementDesc
{
    FString SemanticName;
    uint32 SemanticIndex = 0;
    DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
    uint32 InputSlot = 0;
    uint32 AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
};

struct FVertexLayoutDesc
{
    TArray<FVertexElementDesc> Elements;
    uint32 Stride = 0;
};

// Vertex Shader Stage입니다.
// InputLayout은 PixelShader가 아니라 VS 입력 시그니처에 종속되므로 여기서 소유합니다.
struct FVertexShader
{
    ID3D11VertexShader* Shader = nullptr;
    ID3D11InputLayout* InputLayout = nullptr;
    FShaderReflectionInfo Reflection;

    void Release()
    {
        if (Shader)
        {
            Shader->Release();
            Shader = nullptr;
        }
        if (InputLayout)
        {
            InputLayout->Release();
            InputLayout = nullptr;
        }
    }
};

// Pixel Shader Stage입니다.
// Material Parameter / Texture Binding은 PS Reflection 정보를 보고 처리합니다.
struct FPixelShader
{
    ID3D11PixelShader* Shader = nullptr;
    FShaderReflectionInfo Reflection;

    void Release()
    {
        if (Shader)
        {
            Shader->Release();
            Shader = nullptr;
        }
    }
};

// VS + PS 조합을 구분하는 키입니다.
// Stage Cache와 Program Cache를 분리해서 같은 PS를 여러 VS와 재사용할 수 있게 합니다.
struct FShaderProgramKey
{
    FShaderStageKey VS;
    FShaderStageKey PS;

    bool operator==(const FShaderProgramKey& Other) const
    {
        return VS == Other.VS && PS == Other.PS;
    }
};

struct FShaderProgramKeyHasher
{
    size_t operator()(const FShaderProgramKey& Key) const
    {
        const FShaderStageKeyHasher StageHasher;
        size_t Hash = StageHasher(Key.VS);
        Hash ^= StageHasher(Key.PS) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
        return Hash;
    }
};

// Runtime에서 실제로 바인딩하는 VS + PS 조합입니다.
// Material은 PS와 파라미터만 알고, 어떤 VS를 쓸지는 VertexFactory가 결정합니다.
struct FShaderProgram
{
    FVertexShader* VS = nullptr;
    FPixelShader* PS = nullptr;

    void Bind(ID3D11DeviceContext* Context) const
    {
        if (!Context || !VS || !PS)
        {
            return;
        }

        Context->IASetInputLayout(VS->InputLayout);
        Context->VSSetShader(VS->Shader, nullptr, 0);
        Context->PSSetShader(PS->Shader, nullptr, 0);
    }
};
