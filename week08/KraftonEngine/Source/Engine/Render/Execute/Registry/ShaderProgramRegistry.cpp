// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Execute/Registry/ShaderProgramRegistry.h"

namespace
{
/*
    그래픽스 프로그램 desc에 들어갈 shader stage desc를 만듭니다.
    built-in 셰이더는 현재 VS/PS가 같은 파일을 공유하는 패턴을 기본값으로 사용합니다.
*/
FShaderStageDesc MakeStageDesc(const char* FilePath, const char* EntryPoint)
{
    FShaderStageDesc Desc = {};
    Desc.FilePath         = FilePath;
    Desc.EntryPoint       = EntryPoint;
    return Desc;
}

/*
    VS와 PS가 같은 define set을 쓰는 built-in 그래픽스 프로그램 desc를 생성합니다.
*/
FGraphicsProgramDesc MakeGraphicsProgramDesc(const char* DebugName, const char* FilePath, const char* VSEntry = "VS", const char* PSEntry = "PS")
{
    FGraphicsProgramDesc Desc = {};
    Desc.DebugName            = DebugName;
    Desc.VS                   = MakeStageDesc(FilePath, VSEntry);
    Desc.PS                   = MakeStageDesc(FilePath, PSEntry);
    return Desc;
}

/*
    그래픽스 프로그램의 모든 stage에 compile define을 추가합니다.
    view mode용 define은 VS/PS 양쪽에 들어가도 안전한 값만 등록합니다.
*/
void AddDefine(FGraphicsProgramDesc& Desc, const char* Name, const char* Value)
{
    Desc.VS.Defines.push_back({ Name, Value });
    if (Desc.PS.has_value())
    {
        Desc.PS->Defines.push_back({ Name, Value });
    }
}
} // namespace

/*
    내장 shader key에 대한 그래픽스 프로그램 desc 테이블을 초기화합니다.
*/
void FShaderProgramRegistry::Initialize()
{
    for (FGraphicsProgramDesc& Desc : Descs)
    {
        Desc = {};
    }

    Add(EShaderType::Primitive, MakeGraphicsProgramDesc("Primitive", "Shaders/Editor/Primitive.hlsl"));
    Add(EShaderType::Gizmo, MakeGraphicsProgramDesc("Gizmo", "Shaders/Editor/Gizmo.hlsl"));
    Add(EShaderType::Editor, MakeGraphicsProgramDesc("Editor", "Shaders/Editor/Editor.hlsl"));
    Add(EShaderType::Decal, MakeGraphicsProgramDesc("Decal", "Shaders/Passes/Scene/DecalPass.hlsl"));
    Add(EShaderType::OutlinePostProcess, MakeGraphicsProgramDesc("OutlinePostProcess", "Shaders/Passes/PostProcess/OutlinePostProcessPass.hlsl"));
    Add(EShaderType::FXAA, MakeGraphicsProgramDesc("FXAA", "Shaders/Passes/PostProcess/FXAAPass.hlsl"));
    Add(EShaderType::Font, MakeGraphicsProgramDesc("Font", "Shaders/Editor/ShaderFont.hlsl"));
    Add(EShaderType::OverlayFont, MakeGraphicsProgramDesc("OverlayFont", "Shaders/Editor/ShaderOverlayFont.hlsl"));
    Add(EShaderType::SubUV, MakeGraphicsProgramDesc("SubUV", "Shaders/Editor/ShaderSubUV.hlsl"));
    Add(EShaderType::Billboard, MakeGraphicsProgramDesc("Billboard", "Shaders/Editor/ShaderBillboard.hlsl"));
    Add(EShaderType::HeightFog, MakeGraphicsProgramDesc("HeightFog", "Shaders/Passes/PostProcess/HeightFogPass.hlsl"));
    Add(EShaderType::DepthOnly, MakeGraphicsProgramDesc("DepthOnly", "Shaders/Passes/Scene/DepthOnlyPass.hlsl"));
    Add(EShaderType::LightHitMap, MakeGraphicsProgramDesc("LightHitMap", "Shaders/Passes/PostProcess/LightHitMapPass.hlsl"));

    FGraphicsProgramDesc SceneDepth = MakeGraphicsProgramDesc("SceneDepth", "Shaders/Passes/Scene/ViewModes/NonLitViewModePass.hlsl");
    AddDefine(SceneDepth, "NON_LIT_VIEW_SCENE_DEPTH", "1");
    Add(EShaderType::SceneDepth, SceneDepth);

    FGraphicsProgramDesc NormalView = MakeGraphicsProgramDesc("NormalView", "Shaders/Passes/Scene/ViewModes/NonLitViewModePass.hlsl");
    AddDefine(NormalView, "NON_LIT_VIEW_WORLD_NORMAL", "1");
    Add(EShaderType::NormalView, NormalView);
}

/*
    shader key에 대응하는 그래픽스 프로그램 desc를 찾습니다.
    등록되지 않은 key면 nullptr를 반환합니다.
*/
const FGraphicsProgramDesc* FShaderProgramRegistry::Find(EShaderType InType) const
{
    const uint32 Idx = (uint32)InType;
    if (Idx >= (uint32)EShaderType::MAX || Descs[Idx].VS.FilePath.empty())
    {
        return nullptr;
    }

    return &Descs[Idx];
}

/*
    shader key에 대응하는 desc를 테이블에 기록합니다.
*/
void FShaderProgramRegistry::Add(EShaderType InType, const FGraphicsProgramDesc& Desc)
{
    const uint32 Idx = (uint32)InType;
    if (Idx < (uint32)EShaderType::MAX)
    {
        Descs[Idx] = Desc;
    }
}
