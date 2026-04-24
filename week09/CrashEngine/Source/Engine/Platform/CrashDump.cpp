// 엔진 영역의 세부 동작을 구현합니다.
#include "Engine/Platform/CrashDump.h"
#include "Engine/Platform/Paths.h"

#include <DbgHelp.h>
#include <ctime>
#include <cstdio>

#pragma comment(lib, "DbgHelp.lib")

LONG WINAPI WriteCrashDump(EXCEPTION_POINTERS* ExceptionInfo)
{
    FPaths::CreateDir(FPaths::DumpDir());

    // 타임스탬프 기반 파일명 생성
    WCHAR FileName[MAX_PATH];
    time_t Now = time(nullptr);
    tm LocalTime;
    localtime_s(&LocalTime, &Now);
    swprintf_s(FileName, L"Crash_%04d%02d%02d_%02d%02d%02d.dmp",
               LocalTime.tm_year + 1900, LocalTime.tm_mon + 1, LocalTime.tm_mday,
               LocalTime.tm_hour, LocalTime.tm_min, LocalTime.tm_sec);

    std::wstring DumpPath = FPaths::Combine(FPaths::DumpDir(), FileName);

    HANDLE File = CreateFileW(DumpPath.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (File != INVALID_HANDLE_VALUE)
    {
        MINIDUMP_EXCEPTION_INFORMATION DumpInfo;
        DumpInfo.ThreadId = GetCurrentThreadId();
        DumpInfo.ExceptionPointers = ExceptionInfo;
        DumpInfo.ClientPointers = FALSE;

        MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            File,
            MiniDumpWithDataSegs,
            &DumpInfo,
            nullptr,
            nullptr);

        CloseHandle(File);

        WCHAR Message[MAX_PATH + 64];
        swprintf_s(Message, L"크래시 덤프가 저장되었습니다:\n%s", DumpPath.c_str());
        MessageBoxW(nullptr, Message, L"Crash", MB_OK | MB_ICONERROR);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}
