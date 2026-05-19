#include "Engine/Runtime/Launch.h"
#include <crtdbg.h>
#include <fbxsdk.h>



#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include "ReflectionSystem/ReflectionDatabase.h"

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

        DebugLog(" [Class] %s", Info->ClassName.ToString().c_str());

        // 1. Property Information
        for (const auto& Prop : Info->Properties)
        {
            DebugLog("   |-- [Property] %-25s %-15s (Offset: %4zu, Visible: %s, Editable: %s, Category: %s)",
                     Prop.Type.c_str(),
                     Prop.Name.ToString().c_str(),
                     Prop.Offset,
                     Prop.IsEditorVisible() ? "Yes" : "No",
                     Prop.IsEditorEditable() ? "Yes" : "No",
                     Prop.Category.c_str());
        }

        // 2. GC Tracking Information
        if (!Info->GcPointerOffsets.empty())
        {
            std::string offsetsStr = "   |-- [GC Pointers] Offsets: ";
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
            DebugLog("   |-- [GC Pointers] None");
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

    int major, minor, revision;
    FbxManager::GetFileFormatVersion(major, minor, revision);

    char buffer[128];
    sprintf_s(buffer, "FBX SDK Version: %d.%d.%d\n", major, minor, revision);
    OutputDebugStringA(buffer);
#endif
#endif
    ReflectionDatabase::ResolveDependencies();

    TestReflectionDatabase();

	return Launch(hInstance, nShowCmd);
}
