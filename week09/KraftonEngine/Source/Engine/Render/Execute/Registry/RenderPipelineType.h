#pragma once

// Render/Execute���� �����Ǵ� ���������� ��� �����Դϴ�.
enum class ERenderPipelineType
{
    DefaultRootPipeline,
    EditorRootPipeline,
    ScenePipeline,
    LitPipeline,
    NonLitPipeline,
    DepthOnlyPipeline,
    PostProcessPipeline,
    OverlayPipeline,
    PresentPipeline,
    Outline
};