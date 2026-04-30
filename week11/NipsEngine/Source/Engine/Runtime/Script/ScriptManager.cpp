#include "ScriptManager.h"
#include <Windows.h>
#include <shellapi.h>
#include <filesystem>

namespace fs = std::filesystem;

bool UScriptManager::EditScript(const FString& name)
{
    // 1. 경로 해석
    fs::path absolutePath = name;

    // 2. 파일 존재 확인
    if (!fs::exists(absolutePath))
    {
        MessageBoxA(NULL, "Script file not found", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    // 3. Windows 기본 에디터로 열기 (wide string 버전 사용)
    HINSTANCE hInst = ShellExecuteW(NULL, L"open", absolutePath.c_str(), NULL, NULL, SW_SHOWNORMAL);

    if ((INT_PTR)hInst <= 32)
    {
        MessageBoxA(NULL, "Failed to open script file", "Error", MB_OK | MB_ICONERROR);
        return false;
    }
	return true;
}
