// UI 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"

// SWidget은 모든 UI 위젯이 상속하는 기본 타입입니다.
class SWidget
{
public:
    SWidget() = default;
    virtual ~SWidget() = default;
};
