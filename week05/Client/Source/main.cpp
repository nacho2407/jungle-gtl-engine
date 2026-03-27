#include "Core/FEngine.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
	{
		MessageBox(nullptr, L"CoInitializeEx failed", L"COM Error", MB_OK);
		return -1;
	}

	FEngine Engine;
	if (!Engine.Initialize(hInstance, L"Jungle Client", 1280, 720))
		return -1;

	Engine.Run();
	Engine.Shutdown();

	if (SUCCEEDED(hr) || hr == S_FALSE)
	{
		CoUninitialize();
	}

	return 0;
}
