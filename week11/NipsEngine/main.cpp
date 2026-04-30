#include "Engine/Runtime/Launch.h"
#include <crtdbg.h>
#include <cassert>
#include "sol/sol.hpp"

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
	(void)hPrevInstance;
	(void)lpCmdLine;
#ifdef _MSC_VER
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	//_CrtSetBreakAlloc(1642);
#endif
#endif
    sol::state lua;
    int x = 0;
    lua.set_function("beep", [&x]
                     { ++x; });
    lua.script("beep()");
    assert(x == 1);

	return Launch(hInstance, nShowCmd);
}
