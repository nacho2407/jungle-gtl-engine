#pragma once

#include "Render/Execute/Registry/ShaderProgramTypes.h"
#include "Render/Resources/Shaders/ShaderProgramDesc.h"

/*
    엔진 내장 그래픽스 셰이더 키를 컴파일 desc로 매핑하는 레지스트리입니다.
    ShaderManager는 이 desc만 받아 컴파일과 캐싱을 수행합니다.
*/
class FShaderProgramRegistry
{
public:
    void Initialize();

    const FGraphicsProgramDesc* Find(EShaderType InType) const;

private:
    void Add(EShaderType InType, const FGraphicsProgramDesc& Desc);

private:
    FGraphicsProgramDesc Descs[(uint32)EShaderType::MAX];
};
