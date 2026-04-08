#include "Engine/Runtime/Launch.h"
#include <crtdbg.h>

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
	(void)hPrevInstance;
	(void)lpCmdLine;
#ifdef _MSC_VER
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	//_CrtSetBreakAlloc(9309);
#endif
#endif
	return Launch(hInstance, nShowCmd);
}
