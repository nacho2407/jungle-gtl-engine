#pragma once

// ERenderPipelineType는 렌더 처리에서 사용할 선택지를 정의합니다.
enum class ERenderPipelineType
{
    DefaultRootPipeline,
    EditorRootPipeline,
    ScenePipeline,
    DeferredPipeline,
    DeferredLitPipeline,
    DeferredUnlitPipeline,
    DeferredWorldNormalPipeline,
    DeferredSceneDepthPipeline,
    ForwardPipeline,
    ForwardLitPipeline,
    ForwardUnlitPipeline,
    ForwardWorldNormalPipeline,
    ForwardSceneDepthPipeline,
    PostProcessPipeline,
    OverlayPipeline,
    PresentPipeline,
    OutlinePipeline
};
