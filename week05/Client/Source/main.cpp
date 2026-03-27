#include "Core/FEngine.h"
#include "Platform/Windows/WindowsEngineLaunch.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
	FEngineLaunchConfig Config;
	Config.Title = L"Jungle Client";
	Config.Width = 1280;
	Config.Height = 720;

	FWindowsEngineLaunch Launch;
	return Launch.Run(hInstance, Config);
}
