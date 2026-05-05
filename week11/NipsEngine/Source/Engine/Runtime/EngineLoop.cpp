#include "Engine/Runtime/EngineLoop.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#endif
#if IS_OBJ_VIEWER
#include "Misc/ObjViewer/ObjViewerEngine.h"
#endif
#if !WITH_EDITOR && !IS_OBJ_VIEWER
#include "Engine/Runtime/GameEngine.h"
#endif

#include <objbase.h>

void FEngineLoop::CreateEngine()
{
#if IS_OBJ_VIEWER
	GEngine = UObjectManager::Get().CreateObject<UObjViewerEngine>();
#elif WITH_EDITOR
	GEngine = UObjectManager::Get().CreateObject<UEditorEngine>();
#else
	GEngine = UObjectManager::Get().CreateObject<UGameEngine>();
#endif
}

bool FEngineLoop::Init(HINSTANCE hInstance, int nShowCmd)
{
	(void)nShowCmd;
	
	UE_LOG("Hello, ZZup Engine!");

	const HRESULT ComResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	bComInitialized = SUCCEEDED(ComResult);
	if (FAILED(ComResult) && ComResult != RPC_E_CHANGED_MODE)
	{
		UE_LOG_WARNING("COM initialization failed. WIC texture loading may fail. HRESULT=0x%08X", static_cast<unsigned int>(ComResult));
	}

	if (!Application.Init(hInstance))
	{
		return false;
	}

#if !WITH_EDITOR && !IS_OBJ_VIEWER
	GameSplashScreen.ShowOverWindow(hInstance, Application.GetWindow().GetHWND());
#endif

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

#if WITH_EDITOR || IS_OBJ_VIEWER
	ShaderDirectoryWatcher.Initialize(FPaths::ShaderDir());
#endif

	CreateEngine();
	GEngine->Init(&Application.GetWindow());
	GEngine->SetTimer(&Timer);
	Application.SetOnCloseRequestedCallback([]()
		{
#if WITH_EDITOR
			if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
			{
				return EditorEngine->GetMainPanel().CanCloseEditor();
			}
#endif
			return true;
		});
	GEngine->BeginPlay();

#if !WITH_EDITOR && !IS_OBJ_VIEWER
	GameSplashScreen.Close();
#endif

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

#if WITH_EDITOR || IS_OBJ_VIEWER
		ShaderDirectoryWatcher.Tick();
#endif
	}

	return 0;
}

void FEngineLoop::Shutdown()
{
	GameSplashScreen.Close();

	if (GEngine)
	{
		GEngine->Shutdown();
		UObjectManager::Get().DestroyObject(GEngine);
		GEngine = nullptr;

#if WITH_EDITOR || IS_OBJ_VIEWER
		ShaderDirectoryWatcher.Shutdown();
#endif
	}

	if (bComInitialized)
	{
		CoUninitialize();
		bComInitialized = false;
	}
}
