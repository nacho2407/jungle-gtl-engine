// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/RHI/D3D11/Shaders/GraphicsProgram.h"

#include <comdef.h>
#include <iostream>

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace
{
constexpr const char* GVertexShaderTarget = "vs_5_0";
constexpr const char* GPixelShaderTarget  = "ps_5_0";

/*
    D3D shader signature mask를 input layout DXGI_FORMAT으로 변환합니다.
    현재 엔진 vertex format에서 사용하는 float/int/uint 컴포넌트 조합을 처리합니다.
*/
DXGI_FORMAT MaskToFormat(D3D_REGISTER_COMPONENT_TYPE ComponentType, BYTE Mask)
{
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

/*
    그래픽스 프로그램 소유 리소스를 이동 생성합니다.
    vertex/pixel stage와 input layout 소유권을 함께 넘깁니다.
*/
FGraphicsProgram::FGraphicsProgram(FGraphicsProgram&& Other) noexcept
    : FShaderProgramBase(std::move(Other)), VertexShader(std::move(Other.VertexShader)), PixelShader(std::move(Other.PixelShader)), InputLayout(Other.InputLayout)
{
    Other.InputLayout = nullptr;
}

/*
    그래픽스 프로그램 소유 리소스를 대입 이동합니다.
    기존 stage와 input layout은 먼저 해제합니다.
*/
FGraphicsProgram& FGraphicsProgram::operator=(FGraphicsProgram&& Other) noexcept
{
    if (this != &Other)
    {
        Release();
        FShaderProgramBase::operator=(std::move(Other));
        VertexShader      = std::move(Other.VertexShader);
        PixelShader       = std::move(Other.PixelShader);
        InputLayout       = Other.InputLayout;
        Other.InputLayout = nullptr;
    }
    return *this;
}

/*
    desc에 지정된 VS와 선택적 PS를 컴파일하여 그래픽스 프로그램을 생성합니다.
    모든 임시 COM 객체 생성이 성공한 뒤에만 기존 프로그램을 교체합니다.
*/
bool FGraphicsProgram::Create(ID3D11Device* InDevice, const FGraphicsProgramDesc& InDesc)
{
    if (InDevice == nullptr || InDesc.VS.FilePath.empty() || InDesc.VS.EntryPoint.empty())
    {
        return false;
    }

    ID3DBlob*                              VertexShaderCSO = nullptr;
    ID3DBlob*                              PixelShaderCSO  = nullptr;
    ID3D11VertexShader*                    VS              = nullptr;
    ID3D11PixelShader*                     PS              = nullptr;
    ID3D11InputLayout*                     NewInputLayout  = nullptr;
    TMap<FString, FMaterialParameterInfo*> NewShaderParameterLayout;
    std::unordered_set<std::wstring>       Dependencies;

    auto CleanupTemps = [&]()
    {
        if (NewInputLayout)
        {
            NewInputLayout->Release();
            NewInputLayout = nullptr;
        }
        if (PS)
        {
            PS->Release();
            PS = nullptr;
        }
        if (VS)
        {
            VS->Release();
            VS = nullptr;
        }
        if (VertexShaderCSO)
        {
            VertexShaderCSO->Release();
            VertexShaderCSO = nullptr;
        }
        if (PixelShaderCSO)
        {
            PixelShaderCSO->Release();
            PixelShaderCSO = nullptr;
        }
        for (auto& Pair : NewShaderParameterLayout)
        {
            delete Pair.second;
        }
        NewShaderParameterLayout.clear();
    };

    if (!CompileVertexShader(InDevice, InDesc.VS, &VertexShaderCSO, &VS, Dependencies))
    {
        CleanupTemps();
        return false;
    }

    if (InDesc.PS.has_value() && !CompilePixelShader(InDevice, InDesc.PS.value(), &PixelShaderCSO, &PS, Dependencies))
    {
        CleanupTemps();
        return false;
    }

    if (!CreateInputLayoutFromReflection(InDevice, VertexShaderCSO, &NewInputLayout))
    {
        CleanupTemps();
        return false;
    }

    ExtractCBufferInfo(VertexShaderCSO, NewShaderParameterLayout);
    if (PixelShaderCSO)
    {
        ExtractCBufferInfo(PixelShaderCSO, NewShaderParameterLayout);
    }

    Release();
    VertexShader.Set(VS, VertexShaderCSO->GetBufferSize());
    PixelShader.Set(PS, PixelShaderCSO ? PixelShaderCSO->GetBufferSize() : 0);
    InputLayout           = NewInputLayout;
    ShaderParameterLayout = std::move(NewShaderParameterLayout);

    VS             = nullptr;
    PS             = nullptr;
    NewInputLayout = nullptr;
    NewShaderParameterLayout.clear();

    CleanupTemps();
    return true;
}

/*
    vertex shader stage를 컴파일하고 D3D11 vertex shader 객체를 생성합니다.
    input layout 생성을 위해 VS blob은 호출자에게 반환합니다.
*/
bool FGraphicsProgram::CompileVertexShader(
    ID3D11Device*                     InDevice,
    const FShaderStageDesc&           InDesc,
    ID3DBlob**                        OutVSBlob,
    ID3D11VertexShader**              OutVS,
    std::unordered_set<std::wstring>& OutDependencies) const
{
    if (!CompileShaderBlob(OutVSBlob, InDesc, GVertexShaderTarget, OutDependencies, "Vertex Shader Compile Error"))
    {
        return false;
    }

    const HRESULT Hr = InDevice->CreateVertexShader((*OutVSBlob)->GetBufferPointer(), (*OutVSBlob)->GetBufferSize(), nullptr, OutVS);
    if (FAILED(Hr))
    {
        std::cerr << "Failed to create Vertex Shader (HRESULT: " << Hr << ")" << std::endl;
        return false;
    }

    return true;
}

/*
    pixel shader stage를 컴파일하고 D3D11 pixel shader 객체를 생성합니다.
    PS desc가 없는 프로그램은 이 함수를 호출하지 않습니다.
*/
bool FGraphicsProgram::CompilePixelShader(
    ID3D11Device*                     InDevice,
    const FShaderStageDesc&           InDesc,
    ID3DBlob**                        OutPSBlob,
    ID3D11PixelShader**               OutPS,
    std::unordered_set<std::wstring>& OutDependencies) const
{
    if (!CompileShaderBlob(OutPSBlob, InDesc, GPixelShaderTarget, OutDependencies, "Pixel Shader Compile Error"))
    {
        return false;
    }

    const HRESULT Hr = InDevice->CreatePixelShader((*OutPSBlob)->GetBufferPointer(), (*OutPSBlob)->GetBufferSize(), nullptr, OutPS);

    if (FAILED(Hr))
    {
        std::cerr << "Failed to create Pixel Shader (HRESULT: " << Hr << ")" << std::endl;
        return false;
    }

    return true;
}

/*
    VS input signature를 리플렉션하여 D3D11 input layout을 생성합니다.
    시스템 값 semantic은 vertex buffer layout에 포함하지 않습니다.
*/
bool FGraphicsProgram::CreateInputLayoutFromReflection(ID3D11Device* InDevice, ID3DBlob* InVSBlob, ID3D11InputLayout** OutInputLayout) const
{
    if (InDevice == nullptr || InVSBlob == nullptr || OutInputLayout == nullptr)
    {
        return false;
    }

    *OutInputLayout = nullptr;

    ID3D11ShaderReflection* Reflector = nullptr;
    HRESULT                 Hr        = D3DReflect(InVSBlob->GetBufferPointer(), InVSBlob->GetBufferSize(), IID_ID3D11ShaderReflection, reinterpret_cast<void**>(&Reflector));
    if (FAILED(Hr))
    {
        return false;
    }

    D3D11_SHADER_DESC ShaderDesc;
    Reflector->GetDesc(&ShaderDesc);

    TArray<D3D11_INPUT_ELEMENT_DESC> Elements;
    TArray<std::string>              SemanticNames;
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
        Elem.SemanticName         = SemanticNames.back().c_str();
        Elem.SemanticIndex        = ParamDesc.SemanticIndex;
        Elem.Format               = MaskToFormat(ParamDesc.ComponentType, ParamDesc.Mask);
        Elem.InputSlot            = 0;
        Elem.AlignedByteOffset    = D3D11_APPEND_ALIGNED_ELEMENT;
        Elem.InputSlotClass       = D3D11_INPUT_PER_VERTEX_DATA;
        Elem.InstanceDataStepRate = 0;
        Elements.push_back(Elem);
    }

    bool bSuccess = true;
    if (!Elements.empty())
    {
        Hr = InDevice->CreateInputLayout(Elements.data(), static_cast<UINT>(Elements.size()), InVSBlob->GetBufferPointer(), InVSBlob->GetBufferSize(), OutInputLayout);
        if (FAILED(Hr))
        {
            _com_error Err(Hr);
            OutputDebugStringW((L"CreateInputLayout failed: " + std::wstring(Err.ErrorMessage())).c_str());
            bSuccess = false;
        }
    }

    Reflector->Release();
    return bSuccess;
}

/*
    그래픽스 프로그램의 D3D11 리소스를 모두 해제합니다.
*/
void FGraphicsProgram::Release()
{
    if (InputLayout)
    {
        InputLayout->Release();
        InputLayout = nullptr;
    }
    ReleaseBase();
    PixelShader.Release();
    VertexShader.Release();
}

/*
    그래픽스 프로그램을 입력 조립기, VS, PS stage에 바인딩합니다.
    PS가 없는 프로그램은 nullptr pixel shader를 바인딩합니다.
*/
void FGraphicsProgram::Bind(ID3D11DeviceContext* InDeviceContext) const
{
    if (!InDeviceContext)
    {
        return;
    }

    InDeviceContext->IASetInputLayout(InputLayout);
    InDeviceContext->VSSetShader(VertexShader.Get(), nullptr, 0);
    InDeviceContext->PSSetShader(PixelShader.Get(), nullptr, 0);
}

/*
    그래픽스 프로그램이 제출 가능한 상태인지 반환합니다.
    VS-only 프로그램도 유효하므로 vertex shader 존재 여부만 검사합니다.
*/
bool FGraphicsProgram::IsValid() const
{
    return VertexShader.Get() != nullptr;
}
