#include "FEditorEngine.h"
#include "Platform/Windows/WindowsEngineLaunch.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
	FEngineLaunchConfig Config;
	Config.Title = L"Jungle Editor";
	Config.Width = 1280;
	Config.Height = 720;
	Config.CreateEngine = []()
	{
		return std::make_unique<FEditorEngine>();
	};

	FWindowsEngineLaunch Launch;
	return Launch.Run(hInstance, Config);
}
