#include "EngineLoop.h"

#include "FEngine.h"
#include "Platform/Windows/WindowsApplication.h"
#include "Platform/Windows/WindowsWindow.h"

FEngineLoop::~FEngineLoop() = default;

bool FEngineLoop::PreInit(HINSTANCE hInstance, const FEngineLaunchConfig& InConfig)
{
	Config = InConfig;
	bExitRequested = false;

	if (!InitializeApplication(hInstance))
	{
		return false;
	}

	return CreateEngineInstance();
}

bool FEngineLoop::Init()
{
	return InitializeEngine();
}

void FEngineLoop::Tick()
{
	if (!App || !Engine)
	{
		RequestExit();
		return;
	}

	if (!App->PumpMessages())
	{
		RequestExit();
		return;
	}

	Engine->TickFrame();
}

void FEngineLoop::Exit()
{
	Engine.reset();

	if (App)
	{
		App->Destroy();
		App = nullptr;
	}

	MainWindow = nullptr;
}

void FEngineLoop::RequestExit()
{
	bExitRequested = true;
}

bool FEngineLoop::IsExitRequested() const
{
	return bExitRequested;
}

bool FEngineLoop::InitializeApplication(HINSTANCE hInstance)
{
	App = &FWindowsApplication::Get();
	if (!App->Create(hInstance))
	{
		return false;
	}

	if (!App->CreateMainWindow(Config.Title, Config.Width, Config.Height))
	{
		return false;
	}

	MainWindow = App->GetMainWindow();
	if (MainWindow == nullptr)
	{
		return false;
	}

	App->ShowWindow();
	return true;
}

bool FEngineLoop::CreateEngineInstance()
{
	if (Config.CreateEngine)
	{
		Engine = Config.CreateEngine();
	}
	else
	{
		Engine = std::make_unique<FEngine>();
	}

	return Engine != nullptr;
}

bool FEngineLoop::InitializeEngine() const
{
	if (!Engine || !MainWindow)
	{
		return false;
	}

	FEngineInitArgs InitArgs;
	InitArgs.App = App;
	InitArgs.MainWindow = MainWindow;
	InitArgs.Hwnd = MainWindow->GetHwnd();
	InitArgs.Width = MainWindow->GetWidth();
	InitArgs.Height = MainWindow->GetHeight();

	return Engine->Initialize(InitArgs);
}
