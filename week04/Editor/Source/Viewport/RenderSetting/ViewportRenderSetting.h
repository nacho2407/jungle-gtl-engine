#pragma once

/*
        Viewport에서 Render Setting과 관련된 Input Context와 Viewport에서의 Render Setting의 상태를 관리하는 Controller
        또한 카메라 Transform 자체 저장, Projection, View Matrix 갱신
        Shading Mode
        WireFream, Lit, Unlit
        grid, bounds, icon 표시 여부 등을 총괄합니다.
        -> 입력 처리 계층이라기보다는 상태 보관 + 렌더 반영 계층
*/

class FViewportRenderSetting
{
};
