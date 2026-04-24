// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Render/Execute/Registry/ShaderProgramTypes.h"
#include "Render/Resources/Shaders/ShaderProgramDesc.h"

// FShaderProgramRegistry는 실행 시 필요한 타입과 규칙의 매핑을 보관합니다.
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
