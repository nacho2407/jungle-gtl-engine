#include "Core/CoreMinimal.h"
#include "Core/Path.h"
#include "Launch/Launch.h"

#include <filesystem>

namespace fs = std::filesystem;

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
                    _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
    (void)hPrevInstance;
    (void)lpCmdLine;

#if defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    //_CrtSetBreakAlloc(377);
#endif

    // Path Init =====================================
    wchar_t ExecutablePath[MAX_PATH] = {};
    ::GetModuleFileNameW(nullptr, ExecutablePath, MAX_PATH);

    fs::path AppRoot = fs::path(ExecutablePath).parent_path();
    if (AppRoot.filename() == L"Debug" || AppRoot.filename() == L"Release")
    {
        AppRoot = AppRoot.parent_path();
    }
    if (AppRoot.filename() == L"Bin")
    {
        AppRoot = AppRoot.parent_path();
    }

    FPathConfig PathConfig;
    PathConfig.AppRoot = AppRoot;
    PathConfig.EngineRoot = AppRoot.parent_path() / L"Engine";
    FPaths::Initialize(PathConfig);
    FPaths::EnsureRuntimeDirectories();
    // ===============================================

    return Launch(hInstance, nShowCmd);
}
