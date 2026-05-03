#include "Engine/Runtime/Launch.h"

#include "Engine/Runtime/EngineLoop.h"
#include "Engine/Platform/CrashDump.h"

// 빌드 변종에 맞는 UEngine 서브클래스 헤더만 포함. EngineLoop 자체는 구체 클래스를
// 모르고, 진입점인 이 파일이 팩토리를 만들어 주입한다 (Engine→Editor/Game 의존
// 끊기 위함).
#if IS_OBJ_VIEWER
#include "ObjViewer/ObjViewerEngine.h"
#elif WITH_EDITOR
#include "Editor/EditorEngine.h"
#elif WITH_STANDALONE
#include "Game/GameEngine.h"
#endif

namespace
{
	UEngine* CreateConcreteEngine()
	{
#if IS_OBJ_VIEWER
		return UObjectManager::Get().CreateObject<UObjViewerEngine>();
#elif WITH_EDITOR
		return UObjectManager::Get().CreateObject<UEditorEngine>();
#elif WITH_STANDALONE
		return UObjectManager::Get().CreateObject<UGameEngine>();
#else
		return UObjectManager::Get().CreateObject<UEngine>();
#endif
	}

	int GuardedMain(HINSTANCE hInstance, int nShowCmd)
	{
		FEngineLoop EngineLoop(&CreateConcreteEngine);
		if (!EngineLoop.Init(hInstance, nShowCmd))
		{
			return -1;
		}

		const int ExitCode = EngineLoop.Run();
		EngineLoop.Shutdown();
		return ExitCode;
	}
}

int Launch(HINSTANCE hInstance, int nShowCmd)
{
	__try
	{
		return GuardedMain(hInstance, nShowCmd);
	}
	__except (WriteCrashDump(GetExceptionInformation()))
	{
		return static_cast<int>(GetExceptionCode());
	}
}
