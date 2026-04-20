#include "Render/D3D11/Shaders/Programs/GraphicsShaderProgram.h"
#include "Render/D3D11/Shaders/Stages/ComputeShaderStage.h"

#include "Materials/Material.h"
#include "Profiling/MemoryStats.h"

#include <comdef.h>
#include <iostream>
#include <string>

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

void FVertexShaderStage::Set(ID3D11VertexShader* InShader, size_t InByteSize)
{
    Release();
    Shader = InShader;
    ByteSize = InByteSize;
    if (Shader)
    {
        MemoryStats::AddVertexShaderMemory(static_cast<uint32>(ByteSize));
    }
}

void FVertexShaderStage::Release()
{
    if (Shader)
    {
        MemoryStats::SubVertexShaderMemory(static_cast<uint32>(ByteSize));
        Shader->Release();
        Shader = nullptr;
    }
    ByteSize = 0;
}

FVertexShaderStage::FVertexShaderStage(FVertexShaderStage&& Other) noexcept : Shader(Other.Shader), ByteSize(Other.ByteSize)
{
    Other.Shader = nullptr;
    Other.ByteSize = 0;
}

FVertexShaderStage& FVertexShaderStage::operator=(FVertexShaderStage&& Other) noexcept
{
    if (this != &Other)
    {
        Release();
        Shader = Other.Shader;
        ByteSize = Other.ByteSize;
        Other.Shader = nullptr;
        Other.ByteSize = 0;
    }
    return *this;
}

void FPixelShaderStage::Set(ID3D11PixelShader* InShader, size_t InByteSize)
{
    Release();
    Shader = InShader;
    ByteSize = InByteSize;
    if (Shader)
    {
        MemoryStats::AddPixelShaderMemory(static_cast<uint32>(ByteSize));
    }
}

void FPixelShaderStage::Release()
{
    if (Shader)
    {
        MemoryStats::SubPixelShaderMemory(static_cast<uint32>(ByteSize));
        Shader->Release();
        Shader = nullptr;
    }
    ByteSize = 0;
}

FPixelShaderStage::FPixelShaderStage(FPixelShaderStage&& Other) noexcept : Shader(Other.Shader), ByteSize(Other.ByteSize)
{
    Other.Shader = nullptr;
    Other.ByteSize = 0;
}

FPixelShaderStage& FPixelShaderStage::operator=(FPixelShaderStage&& Other) noexcept
{
    if (this != &Other)
    {
        Release();
        Shader = Other.Shader;
        ByteSize = Other.ByteSize;
        Other.Shader = nullptr;
        Other.ByteSize = 0;
    }
    return *this;
}

void FComputeShaderStage::Set(ID3D11ComputeShader* InShader, size_t InByteSize)
{
    Release();
    Shader = InShader;
    ByteSize = InByteSize;
}

void FComputeShaderStage::Release()
{
    if (Shader)
    {
        Shader->Release();
        Shader = nullptr;
    }
    ByteSize = 0;
}

FComputeShaderStage::FComputeShaderStage(FComputeShaderStage&& Other) noexcept : Shader(Other.Shader), ByteSize(Other.ByteSize)
{
    Other.Shader = nullptr;
    Other.ByteSize = 0;
}

FComputeShaderStage& FComputeShaderStage::operator=(FComputeShaderStage&& Other) noexcept
{
    if (this != &Other)
    {
        Release();
        Shader = Other.Shader;
        ByteSize = Other.ByteSize;
        Other.Shader = nullptr;
        Other.ByteSize = 0;
    }
    return *this;
}

FShader::FShader(FShader&& Other) noexcept
    : VertexShader(std::move(Other.VertexShader)), PixelShader(std::move(Other.PixelShader)), InputLayout(Other.InputLayout), ShaderParameterLayout(std::move(Other.ShaderParameterLayout))
{
    Other.InputLayout = nullptr;
}

FShader& FShader::operator=(FShader&& Other) noexcept
{
    if (this != &Other)
    {
        Release();
        VertexShader = std::move(Other.VertexShader);
        PixelShader = std::move(Other.PixelShader);
        InputLayout = Other.InputLayout;
        ShaderParameterLayout = std::move(Other.ShaderParameterLayout);
        Other.InputLayout = nullptr;
    }
    return *this;
}

void FShader::Create(ID3D11Device* InDevice, const wchar_t* InFilePath, const char* InVSEntryPoint, const char* InPSEntryPoint,
                                    const D3D_SHADER_MACRO* InDefines)
{
    Release();

    ID3DBlob* VertexShaderCSO = nullptr;
    ID3DBlob* PixelShaderCSO = nullptr;
    ID3DBlob* ErrorBlob = nullptr;

    HRESULT hr = D3DCompileFromFile(InFilePath, InDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE, InVSEntryPoint, "vs_5_0", 0, 0, &VertexShaderCSO, &ErrorBlob);
    if (FAILED(hr))
    {
        if (ErrorBlob)
        {
            MessageBoxA(nullptr, static_cast<char*>(ErrorBlob->GetBufferPointer()), "Vertex Shader Compile Error", MB_OK | MB_ICONERROR);
            ErrorBlob->Release();
        }
        return;
    }

    hr = D3DCompileFromFile(InFilePath, InDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE, InPSEntryPoint, "ps_5_0", 0, 0, &PixelShaderCSO, &ErrorBlob);
    if (FAILED(hr))
    {
        if (ErrorBlob)
        {
            MessageBoxA(nullptr, static_cast<char*>(ErrorBlob->GetBufferPointer()), "Pixel Shader Compile Error", MB_OK | MB_ICONERROR);
            ErrorBlob->Release();
        }
        VertexShaderCSO->Release();
        return;
    }

    ID3D11VertexShader* VS = nullptr;
    hr = InDevice->CreateVertexShader(VertexShaderCSO->GetBufferPointer(), VertexShaderCSO->GetBufferSize(), nullptr, &VS);
    if (FAILED(hr))
    {
        std::cerr << "Failed to create Vertex Shader (HRESULT: " << hr << ")" << std::endl;
        VertexShaderCSO->Release();
        PixelShaderCSO->Release();
        return;
    }
    VertexShader.Set(VS, VertexShaderCSO->GetBufferSize());

    ID3D11PixelShader* PS = nullptr;
    hr = InDevice->CreatePixelShader(PixelShaderCSO->GetBufferPointer(), PixelShaderCSO->GetBufferSize(), nullptr, &PS);
    if (FAILED(hr))
    {
        std::cerr << "Failed to create Pixel Shader (HRESULT: " << hr << ")" << std::endl;
        VertexShaderCSO->Release();
        PixelShaderCSO->Release();
        Release();
        return;
    }
    PixelShader.Set(PS, PixelShaderCSO->GetBufferSize());

    CreateInputLayoutFromReflection(InDevice, VertexShaderCSO);
    ExtractCBufferInfo(VertexShaderCSO, ShaderParameterLayout);
    ExtractCBufferInfo(PixelShaderCSO, ShaderParameterLayout);

    VertexShaderCSO->Release();
    PixelShaderCSO->Release();
}

void FShader::Release()
{
    if (InputLayout)
    {
        InputLayout->Release();
        InputLayout = nullptr;
    }
    PixelShader.Release();
    VertexShader.Release();
}

void FShader::Bind(ID3D11DeviceContext* InDeviceContext) const
{
    InDeviceContext->IASetInputLayout(InputLayout);
    InDeviceContext->VSSetShader(VertexShader.Get(), nullptr, 0);
    InDeviceContext->PSSetShader(PixelShader.Get(), nullptr, 0);
}

namespace
{
DXGI_FORMAT MaskToFormat(D3D_REGISTER_COMPONENT_TYPE ComponentType, BYTE Mask)
{
    int Count = 0;
    if (Mask & 0x1) ++Count;
    if (Mask & 0x2) ++Count;
    if (Mask & 0x4) ++Count;
    if (Mask & 0x8) ++Count;

    if (ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
    {
        switch (Count)
        {
        case 1: return DXGI_FORMAT_R32_FLOAT;
        case 2: return DXGI_FORMAT_R32G32_FLOAT;
        case 3: return DXGI_FORMAT_R32G32B32_FLOAT;
        case 4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        }
    }
    else if (ComponentType == D3D_REGISTER_COMPONENT_UINT32)
    {
        switch (Count)
        {
        case 1: return DXGI_FORMAT_R32_UINT;
        case 2: return DXGI_FORMAT_R32G32_UINT;
        case 3: return DXGI_FORMAT_R32G32B32_UINT;
        case 4: return DXGI_FORMAT_R32G32B32A32_UINT;
        }
    }
    else if (ComponentType == D3D_REGISTER_COMPONENT_SINT32)
    {
        switch (Count)
        {
        case 1: return DXGI_FORMAT_R32_SINT;
        case 2: return DXGI_FORMAT_R32G32_SINT;
        case 3: return DXGI_FORMAT_R32G32B32_SINT;
        case 4: return DXGI_FORMAT_R32G32B32A32_SINT;
        }
    }
    return DXGI_FORMAT_UNKNOWN;
}
}

void FShader::CreateInputLayoutFromReflection(ID3D11Device* InDevice, ID3DBlob* VSBlob)
{
    ID3D11ShaderReflection* Reflector = nullptr;
    HRESULT hr = D3DReflect(VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), IID_ID3D11ShaderReflection, reinterpret_cast<void**>(&Reflector));
    if (FAILED(hr))
    {
        return;
    }

    D3D11_SHADER_DESC ShaderDesc;
    Reflector->GetDesc(&ShaderDesc);

    TArray<D3D11_INPUT_ELEMENT_DESC> Elements;
    TArray<std::string> SemanticNames;
    SemanticNames.reserve(ShaderDesc.InputParameters);

    for (UINT i = 0; i < ShaderDesc.InputParameters; ++i)
    {
        D3D11_SIGNATURE_PARAMETER_DESC ParamDesc;
        Reflector->GetInputParameterDesc(i, &ParamDesc);
        if (ParamDesc.SystemValueType != D3D_NAME_UNDEFINED)
        {
            continue;
        }

        D3D11_INPUT_ELEMENT_DESC Elem = {};
        SemanticNames.emplace_back(ParamDesc.SemanticName ? ParamDesc.SemanticName : "");
        Elem.SemanticName = SemanticNames.back().c_str();
        Elem.SemanticIndex = ParamDesc.SemanticIndex;
        Elem.Format = MaskToFormat(ParamDesc.ComponentType, ParamDesc.Mask);
        Elem.InputSlot = 0;
        Elem.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        Elem.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        Elem.InstanceDataStepRate = 0;
        Elements.push_back(Elem);
    }

    if (!Elements.empty())
    {
        hr = InDevice->CreateInputLayout(Elements.data(), static_cast<UINT>(Elements.size()), VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), &InputLayout);
        if (FAILED(hr))
        {
            _com_error Err(hr);
            OutputDebugStringW((L"CreateInputLayout failed: " + std::wstring(Err.ErrorMessage())).c_str());
        }
    }

    Reflector->Release();
}

void FShader::ExtractCBufferInfo(ID3DBlob* ShaderBlob, TMap<FString, FMaterialParameterInfo*>& OutLayout)
{
    ID3D11ShaderReflection* Reflector = nullptr;
    D3DReflect(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), IID_ID3D11ShaderReflection, reinterpret_cast<void**>(&Reflector));

    D3D11_SHADER_DESC ShaderDesc;
    Reflector->GetDesc(&ShaderDesc);

    for (UINT i = 0; i < ShaderDesc.ConstantBuffers; ++i)
    {
        auto* CB = Reflector->GetConstantBufferByIndex(i);
        D3D11_SHADER_BUFFER_DESC CBDesc;
        CB->GetDesc(&CBDesc);

        FString BufferName = CBDesc.Name;
        D3D11_SHADER_INPUT_BIND_DESC BindDesc;
        Reflector->GetResourceBindingDescByName(CBDesc.Name, &BindDesc);
        UINT SlotIndex = BindDesc.BindPoint;
        if (SlotIndex != 2 && SlotIndex != 3)
        {
            continue;
        }

        for (UINT j = 0; j < CBDesc.Variables; ++j)
        {
            auto* Var = CB->GetVariableByIndex(j);
            D3D11_SHADER_VARIABLE_DESC VarDesc;
            Var->GetDesc(&VarDesc);

            FMaterialParameterInfo* Info = new FMaterialParameterInfo();
            Info->BufferName = BufferName;
            Info->SlotIndex = SlotIndex;
            Info->Offset = VarDesc.StartOffset;
            Info->Size = VarDesc.Size;
            Info->BufferSize = CBDesc.Size;
            OutLayout[VarDesc.Name] = Info;
        }
    }

    Reflector->Release();
}
