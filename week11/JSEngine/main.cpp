#include "Engine/Runtime/Launch.h"
#include <crtdbg.h>
#include <fbxsdk.h>



#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include "Core/ReflectionDatabase.h"

// Helper function to send formatted strings to Visual Studio Output window
void DebugLog(const char* format, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsprintf_s(buffer, format, args);
    va_end(args);

    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");
}

void TestReflectionDatabase()
{
    DebugLog("======================================================");
    DebugLog("        [Reflection Database Loading Test]           ");
    DebugLog("======================================================");

    const auto& AllClasses = ReflectionDatabase::GetAllClasses();
    DebugLog("Total Registered Classes: %zu", AllClasses.size());
    DebugLog("------------------------------------------------------");

    for (const auto& Pair : AllClasses)
    {
        FClassInfo* Info = Pair.second;
        if (!Info)
            continue;

        DebugLog("📁 [Class] %s", Info->ClassName.ToString().c_str());

        // 1. Property Information
        for (const auto& Prop : Info->Properties)
        {
            DebugLog("   ├── [Property] %-25s %-15s (Offset: %4zu, Edit: %s)",
                     Prop.Type.c_str(),
                     Prop.Name.ToString().c_str(),
                     Prop.Offset,
                     Prop.bIsEditAnywhere ? "Yes" : "No");
        }

        // 2. GC Tracking Information
        if (!Info->GcPointerOffsets.empty())
        {
            std::string offsetsStr = "   └── [GC Pointers] Offsets: ";
            for (size_t i = 0; i < Info->GcPointerOffsets.size(); ++i)
            {
                offsetsStr += std::to_string(Info->GcPointerOffsets[i]);
                if (i < Info->GcPointerOffsets.size() - 1)
                    offsetsStr += ", ";
            }
            DebugLog("%s", offsetsStr.c_str());
        }
        else
        {
            DebugLog("   └── [GC Pointers] None");
        }
        DebugLog("------------------------------------------------------");
    }
}



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
    TestReflectionDatabase();

	return Launch(hInstance, nShowCmd);
}
