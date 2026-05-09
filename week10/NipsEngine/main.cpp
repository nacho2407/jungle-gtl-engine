#include "Engine/Runtime/Launch.h"
#include <crtdbg.h>
#include <fbxsdk.h>

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
#ifdef _MSC_VER
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    // _CrtSetBreakAlloc(23304399);

	FbxManager* manager = FbxManager::Create();
    if (!manager)
    {
        OutputDebugStringA("FbxManager Creation Failed\n");
        return -1;
    }

    OutputDebugStringA("FbxManager Creation OK\n");

    FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
    manager->SetIOSettings(ios);

    OutputDebugStringA("IOSettings Creation OK\n");

    int major, minor, revision;
    FbxManager::GetFileFormatVersion(major, minor, revision);

    char buffer[128];
    sprintf_s(buffer, "FBX SDK Version: %d.%d.%d\n", major, minor, revision);
    OutputDebugStringA(buffer);

    manager->Destroy();

    OutputDebugStringA("FBX Log End\n");
#endif
#endif
	return Launch(hInstance, nShowCmd);
}
