#include "Render/Hardware/Resources/Shader.h"
#include "Profiling/MemoryStats.h"
#include "Materials/Material.h"
#include <iostream>
#include <string>
#include <comdef.h>
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

FShader::FShader(FShader&& Other) noexcept
    : VertexShader(Other.VertexShader), PixelShader(Other.PixelShader), InputLayout(Other.InputLayout), ShaderParameterLayout(std::move(Other.ShaderParameterLayout))
{
    Other.VertexShader = nullptr;
    Other.PixelShader = nullptr;
    Other.InputLayout = nullptr;
}

FShader& FShader::operator=(FShader&& Other) noexcept
{
    if (this != &Other)
    {
        Release();
        VertexShader = Other.VertexShader;
        PixelShader = Other.PixelShader;
        InputLayout = Other.InputLayout;
        Other.VertexShader = nullptr;
        Other.PixelShader = nullptr;
        Other.InputLayout = nullptr;
    }
    return *this;
}

void FShader::Create(ID3D11Device* InDevice, const wchar_t* InFilePath, const char* InVSEntryPoint, const char* InPSEntryPoint,
                     const D3D_SHADER_MACRO* InDefines)
{
    Release();

    ID3DBlob* vertexShaderCSO = nullptr;
    ID3DBlob* pixelShaderCSO = nullptr;
    ID3DBlob* errorBlob = nullptr;

    // Vertex Shader 컴파일
    HRESULT hr = D3DCompileFromFile(InFilePath, InDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE, InVSEntryPoint, "vs_5_0", 0, 0, &vertexShaderCSO, &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            MessageBoxA(nullptr, (char*)errorBlob->GetBufferPointer(), "Vertex Shader Compile Error", MB_OK | MB_ICONERROR);
            errorBlob->Release();
        }
        return;
    }

    // Pixel Shader 컴파일
    hr = D3DCompileFromFile(InFilePath, InDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE, InPSEntryPoint, "ps_5_0", 0, 0, &pixelShaderCSO, &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            MessageBoxA(nullptr, (char*)errorBlob->GetBufferPointer(), "Pixel Shader Compile Error", MB_OK | MB_ICONERROR);
            errorBlob->Release();
        }
        vertexShaderCSO->Release();
        return;
    }

    // Vertex Shader 생성
    hr = InDevice->CreateVertexShader(vertexShaderCSO->GetBufferPointer(), vertexShaderCSO->GetBufferSize(), nullptr, &VertexShader);
    if (FAILED(hr))
    {
        std::cerr << "Failed to create Vertex Shader (HRESULT: " << hr << ")" << std::endl;
        vertexShaderCSO->Release();
        pixelShaderCSO->Release();
        return;
    }

    CachedVertexShaderSize = vertexShaderCSO->GetBufferSize();
    MemoryStats::AddVertexShaderMemory(static_cast<uint32>(CachedVertexShaderSize));

    // Pixel Shader 생성
    hr = InDevice->CreatePixelShader(pixelShaderCSO->GetBufferPointer(), pixelShaderCSO->GetBufferSize(), nullptr, &PixelShader);
    if (FAILED(hr))
    {
        std::cerr << "Failed to create Pixel Shader (HRESULT: " << hr << ")" << std::endl;
        Release();
        vertexShaderCSO->Release();
        pixelShaderCSO->Release();
        return;
    }

    CachedPixelShaderSize = pixelShaderCSO->GetBufferSize();
    MemoryStats::AddPixelShaderMemory(static_cast<uint32>(CachedPixelShaderSize));

    // Input Layout 생성 (VS input signature로부터 자동 추출)
    CreateInputLayoutFromReflection(InDevice, vertexShaderCSO);

    ExtractCBufferInfo(vertexShaderCSO, ShaderParameterLayout);
    ExtractCBufferInfo(pixelShaderCSO, ShaderParameterLayout);

    vertexShaderCSO->Release();
    pixelShaderCSO->Release();
}

void FShader::Release()
{
    if (InputLayout)
    {
        InputLayout->Release();
        InputLayout = nullptr;
    }
    if (PixelShader)
    {
        MemoryStats::SubPixelShaderMemory(static_cast<uint32>(CachedPixelShaderSize));
        CachedPixelShaderSize = 0;

        PixelShader->Release();
        PixelShader = nullptr;
    }
    if (VertexShader)
    {
        MemoryStats::SubVertexShaderMemory(static_cast<uint32>(CachedVertexShaderSize));
        CachedVertexShaderSize = 0;

        VertexShader->Release();
        VertexShader = nullptr;
    }
}

void FShader::Bind(ID3D11DeviceContext* InDeviceContext) const
{
    InDeviceContext->IASetInputLayout(InputLayout);
    InDeviceContext->VSSetShader(VertexShader, nullptr, 0);
    InDeviceContext->PSSetShader(PixelShader, nullptr, 0);
}


namespace
{
DXGI_FORMAT MaskToFormat(D3D_REGISTER_COMPONENT_TYPE ComponentType, BYTE Mask)
{
    // Mask 비트 수 세기 (사용되는 컴포넌트 개수)
    int Count = 0;
    if (Mask & 0x1)
        ++Count;
    if (Mask & 0x2)
        ++Count;
    if (Mask & 0x4)
        ++Count;
    if (Mask & 0x8)
        ++Count;

    if (ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
    {
        switch (Count)
        {
        case 1:
            return DXGI_FORMAT_R32_FLOAT;
        case 2:
            return DXGI_FORMAT_R32G32_FLOAT;
        case 3:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case 4:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        }
    }
    else if (ComponentType == D3D_REGISTER_COMPONENT_UINT32)
    {
        switch (Count)
        {
        case 1:
            return DXGI_FORMAT_R32_UINT;
        case 2:
            return DXGI_FORMAT_R32G32_UINT;
        case 3:
            return DXGI_FORMAT_R32G32B32_UINT;
        case 4:
            return DXGI_FORMAT_R32G32B32A32_UINT;
        }
    }
    else if (ComponentType == D3D_REGISTER_COMPONENT_SINT32)
    {
        switch (Count)
        {
        case 1:
            return DXGI_FORMAT_R32_SINT;
        case 2:
            return DXGI_FORMAT_R32G32_SINT;
        case 3:
            return DXGI_FORMAT_R32G32B32_SINT;
        case 4:
            return DXGI_FORMAT_R32G32B32A32_SINT;
        }
    }
    return DXGI_FORMAT_UNKNOWN;
}
} // namespace

void FShader::CreateInputLayoutFromReflection(ID3D11Device* InDevice, ID3DBlob* VSBlob)
{
    ID3D11ShaderReflection* Reflector = nullptr;
    HRESULT hr = D3DReflect(VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(),
                            IID_ID3D11ShaderReflection, (void**)&Reflector);
    if (FAILED(hr))
        return;

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
            continue;

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
        hr = InDevice->CreateInputLayout(Elements.data(), static_cast<UINT>(Elements.size()),
                                         VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), &InputLayout);
        if (FAILED(hr))
        {
            _com_error Err(hr);
            OutputDebugStringW((L"CreateInputLayout failed: " + std::wstring(Err.ErrorMessage()) + L"").c_str());
        }
    }

    Reflector->Release();
}

// 셰이더 컴파일 후 호출. 셰이더 코드의 cbuffer, 텍스처 샘플러 선언을 분석해서 outlayout에 채워넣음. 이 정보는 머티리얼 템플릿이 생성될 때 참조되어야 하므로 셰이더 내부에서 제공하는 형태로 존재해야 함.
void FShader::ExtractCBufferInfo(ID3DBlob* ShaderBlob, TMap<FString, FMaterialParameterInfo*>& OutLayout)
{
    ID3D11ShaderReflection* Reflector = nullptr;
    D3DReflect(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(),
               IID_ID3D11ShaderReflection, (void**)&Reflector);

    D3D11_SHADER_DESC ShaderDesc;
    Reflector->GetDesc(&ShaderDesc);

    for (UINT i = 0; i < ShaderDesc.ConstantBuffers; ++i)
    {
        auto* CB = Reflector->GetConstantBufferByIndex(i);
        D3D11_SHADER_BUFFER_DESC CBDesc;
        CB->GetDesc(&CBDesc);

        FString BufferName = CBDesc.Name; // "PerMaterial", "PerFrame" 등

        // 상수 버퍼의 바인딩 정보(Slot Index) 가져오기
        D3D11_SHADER_INPUT_BIND_DESC BindDesc;
        Reflector->GetResourceBindingDescByName(CBDesc.Name, &BindDesc);
        UINT SlotIndex = BindDesc.BindPoint; // 이것이 b0, b1의 숫자입니다.

        if (SlotIndex != 2 && SlotIndex != 3) // b2, b3만 저장
            continue;

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

            Info->BufferSize = CBDesc.Size; // cbuffer 크기

            OutLayout[VarDesc.Name] = Info;
        }
    }
    Reflector->Release();
}
