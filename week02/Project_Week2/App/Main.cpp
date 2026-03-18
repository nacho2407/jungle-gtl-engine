#include <cstdio>
#include <windows.h>

#include "FApplication.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    // 콘솔창 띄우기
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);

    FApplication App;

    if (!App.Initialize(hInstance))
    {
        return -1;
    }

    return App.Run();
}