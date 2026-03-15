#include <windows.h>
#include "FApplication.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    FApplication App;

    if (!App.Initialize(hInstance))
    {
        return -1;
    }

    return App.Run();
}