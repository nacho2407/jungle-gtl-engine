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

	Application.SetOnSizingCallback([this]() { TickFrame(); });
	Application.SetOnResizedCallback([](unsigned int Width, unsigned int Height)
	{
		if (GEngine)
		{
			GEngine->OnWindowResized(Width, Height);
		}
	});

	CreateEngine();
	GEngine->Init(&Application.GetWindow());
	GEngine->BeginPlay();

	InitializeTiming();
	return true;
}

void FEngineLoop::InitializeTiming()
{
	QueryPerformanceFrequency(&Frequency);
	QueryPerformanceCounter(&PrevTime);
	DeltaTime = 0.0f;
}

void FEngineLoop::TickFrame()
{
	QueryPerformanceCounter(&CurrTime);
	DeltaTime = static_cast<float>(CurrTime.QuadPart - PrevTime.QuadPart) / static_cast<float>(Frequency.QuadPart);
	PrevTime = CurrTime;

	float MainLoopFps = (DeltaTime > 1e-6f) ? (1.0f / DeltaTime) : 0.0f;
	GEngine->SetMainLoopFPS(MainLoopFps);

	if (Application.IsResizing())
	{
		GEngine->Tick(DeltaTime);
		GEngine->Render(DeltaTime);
		return;
	}

	GEngine->BeginFrame(DeltaTime);
	GEngine->Tick(DeltaTime);
	GEngine->Render(DeltaTime);
	GEngine->EndFrame();
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

		TickFrame();
	}

	return 0;
}

void FEngineLoop::Shutdown()
{
	if (GEngine)
	{
		GEngine->Shutdown();
	}
	UObjectManager::Get().CollectGarbage();
	GEngine = nullptr;
}
