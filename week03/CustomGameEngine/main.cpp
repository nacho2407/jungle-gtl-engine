#include "EngineApp.h"

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR IpCmdLine, int nShowCmd) 
{
	EngineApp* app = new EngineApp();
	
	if (!app->Initialize(hInstance))
		return -1;

	app->Run();
	app->Finalize();

	delete app;

	return 0;
}