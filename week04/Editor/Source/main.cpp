#include "Core/CoreMinimal.h"
#include "Core/Path.h"

#include <filesystem>

namespace fs = std::filesystem;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nShowCmd)
{
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

	return 0;
}
