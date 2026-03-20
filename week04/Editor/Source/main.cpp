#include "Core/CoreMinimal.h"

#include "Launch/Launch.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nShowCmd)
{  
    return Launch(hInstance, nShowCmd);
}
