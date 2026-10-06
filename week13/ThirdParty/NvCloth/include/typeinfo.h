#pragma once

// NvCloth 1.1 헤더의 MSVC 호환성 보강
// 최신 MSVC toolset에는 구형 C++ 헤더 이름인 <typeinfo.h>가 없으므로
// NvCloth include 경로 안에서 표준 <typeinfo>로 우회
#include <typeinfo>
