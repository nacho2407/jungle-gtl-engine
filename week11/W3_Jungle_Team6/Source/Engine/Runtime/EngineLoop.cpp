#include "Engine/Runtime/EngineLoop.h"

#include "Editor/EditorEngine.h"

void FEngineLoop::CreateEngine()
{
#if WITH_EDITOR
	GEngine = UObjectManager::Get().CreateObject<UEditorEngine>();
#else
	GEngine = UObjectManager::Get().CreateObject<UEngine>();
#endif
}

bool FEngineLoop::Init(HINSTANCE hInstance, int nShowCmd)
{
	(void)nShowCmd;

	if (!Application.Init(hInstance))
	{
		return false;
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
	GEngine->Init(&Application.GetWindow());
	GEngine->SetTimer(&Timer);
	GEngine->BeginPlay();

	Timer.Initialize();

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
	}
	GEngine = nullptr;
}
