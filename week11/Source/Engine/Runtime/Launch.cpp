#include "Engine/Runtime/Launch.h"

#include "Engine/Runtime/EngineLoop.h"
#include "Engine/Core/CrashDump.h"
#include "Object/FName.h"
#include "ReflectionSystem/ReflectionDatabase.h"
#include "ReflectionSystem/ReflectionUtils.h"

namespace
{
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

int GuardedMain(HINSTANCE hInstance, int nShowCmd)
{
    ReflectionDatabase::ResolveDependencies();

    TestReflectionDatabase();

    FEngineLoop EngineLoop;
    if (!EngineLoop.Init(hInstance, nShowCmd))
    {
        return -1;
    }

    const int ExitCode = EngineLoop.Run();
    EngineLoop.Shutdown();
    ReflectionDatabase::Shutdown();
    ReflectionUtils::ClearStablePropertyNameCache();
    FNamePool::Get().Clear();
    return ExitCode;
}
} // namespace

int Launch(HINSTANCE hInstance, int nShowCmd)
{
    __try
    {
        return GuardedMain(hInstance, nShowCmd);
    }
    __except (ReportCrash(GetExceptionInformation()))
    {
        return static_cast<int>(GetExceptionCode());
    }
}
