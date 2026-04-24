// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

// ERenderPipelineType는 렌더 처리에서 사용할 선택지를 정의합니다.
enum class ERenderPipelineType
{
    DefaultRootPipeline,
    EditorRootPipeline,
    ScenePipeline,
    LitPipeline,
    UnlitPipeline,
    WorldNormalPipeline,
    SceneDepthPipeline,
    PostProcessPipeline,
    OverlayPipeline,
    PresentPipeline,
    Outline
};
