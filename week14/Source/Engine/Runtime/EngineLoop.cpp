#include "Engine/Runtime/EngineLoop.h"
#include "Profiling/StartupProfiler.h"

FEngineLoop::FEngineLoop(FCreateEngineFn InEngineFactory)
	: EngineFactory(InEngineFactory)
{
}

void FEngineLoop::CreateEngine()
{
	// 팩토리 미주입 시 베이스 UEngine 으로 fallback — 구체 변종 선택은 호출 측 책임.
	GEngine = EngineFactory ? EngineFactory() : UObjectManager::Get().CreateObject<UEngine>();
}

bool FEngineLoop::Init(HINSTANCE hInstance, int nShowCmd)
{
	{
		SCOPE_STARTUP_STAT("WindowsApplication::Init");
		if (!Application.Init(hInstance))
		{
			return false;
		}
	}

	Application.SetOnSizingCallback([this]()
		{
			Timer.Tick();
			GEngine->Tick(Timer.GetDeltaTime());
		});

	Application.SetOnResizedCallback([](unsigned int Width, unsigned int Height)
		{
			if (GEngine)
			{
				GEngine->OnWindowResized(Width, Height);
			}
		});

	CreateEngine();

	{
		SCOPE_STARTUP_STAT("Engine::Init");
		GEngine->Init(&Application.GetWindow());
	}

	GEngine->SetTimer(&Timer);

	{
		SCOPE_STARTUP_STAT("Engine::BeginPlay");
		GEngine->BeginPlay();
	}

	Timer.Initialize();
	FStartupProfiler::Get().Finish();

	return true;
}

int FEngineLoop::Run()
{
	while (!Application.IsExitRequested())
	{
		Application.PumpMessages();

		if (Application.IsExitRequested())
		{
			break;
		}

		Timer.Tick();
		GEngine->Tick(Timer.GetDeltaTime());
	}

	return 0;
}

void FEngineLoop::Shutdown()
{
	if (GEngine)
	{
		GEngine->Shutdown();
		UObjectManager::Get().DestroyObject(GEngine);
		GEngine = nullptr;
	}
}
