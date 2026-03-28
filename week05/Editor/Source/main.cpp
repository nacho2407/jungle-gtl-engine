#include "FEditorEngine.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
	{
		MessageBox(nullptr, L"CoInitializeEx failed", L"COM Error", MB_OK);
		return -1;
	}

	FEditorEngine Engine;
	if (!Engine.Initialize(hInstance))
		return -1;

	Engine.Run();
	Engine.Shutdown(); // ~FEingine() called shutdown

	if (SUCCEEDED(hr) || hr == S_FALSE)
	{
		CoUninitialize();
	}

	return 0;
}
