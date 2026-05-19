// ReflectionMacros.h

// 우리가 리플렉션 툴(Clang)로 이 코드를 분석할 때만 __clang__ 이 정의됩니다.
#ifdef __clang__
// Clang 파서에게 "이 변수에 IGNORE_REFLECTION 이라는 꼬리표를 붙여라"라고 명령합니다.
#define IGNORE_REFLECTION __attribute__((annotate("IGNORE_REFLECTION")))
#else
// 실제 게임 빌드(Visual Studio MSVC 등) 시에는 아무것도 없는 빈 칸으로 취급합니다.
#define IGNORE_REFLECTION
#endif