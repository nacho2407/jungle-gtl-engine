#include "Engine/Runtime/Launch.h"
#include <crtdbg.h>
#include <fbxsdk.h>

#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
#ifdef _MSC_VER
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    // _CrtSetBreakAlloc(23304399);

    int major, minor, revision;
    FbxManager::GetFileFormatVersion(major, minor, revision);

    char buffer[128];
    sprintf_s(buffer, "FBX SDK Version: %d.%d.%d\n", major, minor, revision);
    OutputDebugStringA(buffer);
#endif
#endif

	return Launch(hInstance, nShowCmd);
}
