#include "FEngine.h"
#include "Platform/Windows/WindowsWindow.h"
#include "Core/ViewportClient.h"
#include "Platform/Windows/WindowsApplication.h"
#include "Object/ObjectGlobals.h"

FEngine* GEngine = nullptr;

FEngine::~FEngine()
{
	Shutdown();
}

bool FEngine::Initialize(const FEngineInitArgs& Args)
{
	App = Args.App;
	MainWindow = Args.MainWindow;

	if (!App || !MainWindow || !Args.Hwnd)
	{
		return false;
	}

	GEngine = this;

	PreInitialize();

	Runtime = std::make_unique<FEngineRuntime>();
	if (!Runtime->Initialize(Args.Hwnd, Args.Width, Args.Height, GetStartupSceneType()))
	{
		return false;
	}

	ViewportClient = CreateViewportClient();
	Runtime->SetViewportClient(ViewportClient.get());

	PostInitialize();

	App->AddMessageFilter(std::bind(&FEngine::OnInput, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	App->SetOnResizeCallback(std::bind(&FEngine::OnResize, this, std::placeholders::_1, std::placeholders::_2));

	return true;
}

void FEngine::TickFrame()
{
	if (Runtime)
	{
		Tick(Runtime->GetTimer().GetDeltaTime());
		Runtime->Tick();
	}
}

bool FEngine::OnInput(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	if (Runtime)
	{
		Runtime->ProcessInput(Hwnd, Msg, WParam, LParam);
	}
	return false;
}

void FEngine::OnResize(int32 Width, int32 Height)
{
	if (Runtime)
	{
		Runtime->OnResize(Width, Height);
	}
}

std::unique_ptr<IViewportClient> FEngine::CreateViewportClient()
{
	return std::make_unique<CGameViewportClient>();
}

void FEngine::Shutdown()
{
	if (GEngine == this)
	{
		GEngine = nullptr;
	}

	if (Runtime)
	{
		Runtime->SetViewportClient(nullptr);
		Runtime->Release();
		Runtime.reset();
	}

	ViewportClient.reset();

	App = nullptr;
	MainWindow = nullptr;
}
