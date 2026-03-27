#include <Windows.h>
#include "Engine/Runtime/Launch.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nShowCmd)
{
	return Launch(hInstance, nShowCmd);
}
