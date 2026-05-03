#include "Engine/Runtime/EngineInitHooks.h"

TArray<FEngineInitHooks::FInitFn>& FEngineInitHooks::GetRegistry()
{
	// 함수 로컬 static — 첫 호출 시점에 초기화. 다른 TU 의 static initializer 가
	// Register() 를 호출해도 GetRegistry() 가 그 시점에 안전하게 instance 생성.
	static TArray<FInitFn> Instance;
	return Instance;
}

void FEngineInitHooks::Register(FInitFn Fn)
{
	if (Fn) GetRegistry().push_back(Fn);
}

void FEngineInitHooks::RunAll()
{
	// 등록 순서대로 실행. 한 init 함수가 다른 모듈을 노출하기 위해 또 Register 호출하면
	// 그것은 다음 RunAll 까지 미룬다 (현재 구현은 vector 끝부분에 추가될 뿐 무한루프 X).
	auto& Registry = GetRegistry();
	for (size_t i = 0; i < Registry.size(); ++i)
	{
		if (Registry[i]) Registry[i]();
	}
}
