#pragma once

/*
    렌더러가 조립해 실행하는 파이프라인 종류입니다.
    루트 파이프라인과 하위 파이프라인을 같은 enum으로 식별합니다.
*/
enum class ERenderPipelineType
{
    DefaultScene,
    EditorScene,
    Scene,
    SceneMain,
    ScenePostProcess,
    EditorOverlay,
    Outline
};
