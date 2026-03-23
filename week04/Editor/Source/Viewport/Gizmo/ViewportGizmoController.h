#pragma once
#include "Engine/ViewPort/ViewportController.h"

/*
        Gizmo에 대한 Input Context와 Viewport에서의 Gizmo의 상태를 관리하는 Controller
        축 Hover, drag begin, update, end
        translate, rotate, scale 적용
        snapping 적용
        -> GizmoInputContext에서 입력을 받아서 처리
*/

class FViewportGizmoController : public Engine::Viewport::IViewportController
{
};
