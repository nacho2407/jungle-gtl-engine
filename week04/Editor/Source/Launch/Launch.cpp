#include "Launch.h"
#include "Launch/EditorEngineLoop.h"

namespace 
{
    int GuardedMain(HINSTANCE HInstance, int NCmdShow)
    {
        FEditorEngineLoop EditorEngineLoop;
        
        if (!EditorEngineLoop.PreInit(HInstance, NCmdShow))
        {
            //  Error Code
            return -1;
        }
        
        const int32 ExitCode = EditorEngineLoop.Run();
        
        EditorEngineLoop.Shutdown();
        
        return ExitCode;
    }
}

int Launch(HINSTANCE HInstance, int NCmdShow)
{
    return GuardedMain(HInstance, NCmdShow);
}