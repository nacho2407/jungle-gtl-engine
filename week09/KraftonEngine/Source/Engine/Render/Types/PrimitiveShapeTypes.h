#pragma once

#include "Core/CoreTypes.h"

/*
    기본 프리미티브 메시 종류를 정의하는 enum입니다.
    주로 메시 버퍼 매니저가 내부 기본 메시를 조회할 때 사용합니다.
*/
enum class EMeshShape
{
    Cube,
    Sphere,
    Plane,
    Quad,
    TexturedQuad,
    TransGizmo,
    RotGizmo,
    ScaleGizmo,
};
