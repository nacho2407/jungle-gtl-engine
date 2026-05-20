#include "Engine/Core/CrashDump.h"
#include "Engine/Core/Paths.h"

#include <DbgHelp.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <cstdio>
#include <iterator>
#include "SimpleJSON/json.hpp"
#pragma comment(lib, "DbgHelp.lib")
LONG WINAPI WriteCrashDump(EXCEPTION_POINTERS* ExceptionInfo, const FCrashReportInfo& Info)
{
    HANDLE File = CreateFileW(
        Info.DumpPath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (File == INVALID_HANDLE_VALUE)
    {
        return EXCEPTION_EXECUTE_HANDLER;
    }

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
    return EXCEPTION_EXECUTE_HANDLER;
}
void WriteCrashLog(EXCEPTION_POINTERS* ExceptionInfo, const FCrashReportInfo& Info)
{
    FPaths::CreateDir(FPaths::DumpDir());

    WCHAR FileName[MAX_PATH];
    time_t Now = time(nullptr);
    tm LocalTime;
    localtime_s(&LocalTime, &Now);
    swprintf_s(FileName, L"CrashLog_%04d%02d%02d_%02d%02d%02d.txt",
               LocalTime.tm_year + 1900, LocalTime.tm_mon + 1, LocalTime.tm_mday,
               LocalTime.tm_hour, LocalTime.tm_min, LocalTime.tm_sec);

    std::filesystem::path LogPath(FPaths::Combine(FPaths::DumpDir(), FileName));
    std::ofstream LogFile(Info.LogPath, std::ios::out | std::ios::app);
    if (!LogFile.is_open())
        return;

    HANDLE Process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    if (!SymInitialize(Process, nullptr, TRUE))
    {
        LogFile << "========================================\n";
        LogFile << "[Engine Crash Report]\n";
        LogFile << "Exception Code: 0x" << std::hex
                << ExceptionInfo->ExceptionRecord->ExceptionCode << std::dec << "\n";
        LogFile << "Call Stack: (symbol init failed)\n";
        LogFile << "========================================\n\n";
        LogFile.close();
        return;
    }

    LogFile << "========================================\n";
    LogFile << "[Engine Crash Report]\n";
    LogFile << "Exception Code: 0x" << std::hex
            << ExceptionInfo->ExceptionRecord->ExceptionCode << std::dec << "\n";
    LogFile << "Call Stack:\n";

    const int MaxFrames = 62;
    void* Stack[MaxFrames];
    WORD Frames = CaptureStackBackTrace(0, MaxFrames, Stack, nullptr);

    char SymbolBuffer[sizeof(SYMBOL_INFO) + 256];
    SYMBOL_INFO* Symbol = reinterpret_cast<SYMBOL_INFO*>(SymbolBuffer);
    Symbol->MaxNameLen = 255;
    Symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    IMAGEHLP_LINE64 Line;
    Line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    DWORD Displacement = 0;

    for (WORD i = 0; i < Frames; ++i)
    {
        DWORD64 Address = reinterpret_cast<DWORD64>(Stack[i]);
        if (SymFromAddr(Process, Address, 0, Symbol))
        {
            if (SymGetLineFromAddr64(Process, Address, &Displacement, &Line))
            {
                LogFile << "[" << i << "] " << Symbol->Name << " (" << Line.FileName
                        << " : " << Line.LineNumber << ")\n";
            }
            else
            {
                LogFile << "[" << i << "] " << Symbol->Name << " (Address: 0x"
                        << std::hex << Address << std::dec << ")\n";
            }
        }
        else
        {
            LogFile << "[" << i << "] (Address: 0x" << std::hex << Address << std::dec
                    << ")\n";
        }
    }

    LogFile << "========================================\n\n";
    LogFile.close();

    SymCleanup(Process);
}

void WriteCrashMetadata(EXCEPTION_POINTERS* ExceptionInfo, const FCrashReportInfo& Info)
{
    json::JSON Root = json::JSON::Make(json::JSON::Class::Object);

    Root["Version"] = 1;
    Root["CrashId"] = Info.CrashId;
    Root["Time"] = Info.TimeString;
    Root["ExceptionCode"] = Info.ExceptionCodeString;

    Root["DumpPath"] = FPaths::ToRelativeString(Info.DumpPath);
    Root["LogPath"] = FPaths::ToRelativeString(Info.LogPath);
    Root["MetaPath"] = FPaths::ToRelativeString(Info.MetaPath);

    // 있으면 유용한 추가 정보
    Root["ProcessId"] = static_cast<int>(GetCurrentProcessId());
    Root["ThreadId"] = static_cast<int>(GetCurrentThreadId());

    // JSON 문자열 생성 (들여쓰기 4칸)
    std::string JsonString = Root.dump(4);

    // 1. 개별 메타 파일 저장 (고유 CrashId 기반)
    std::ofstream MetaFile(Info.MetaPath);
    if (MetaFile.is_open())
    {
        MetaFile << JsonString;
        MetaFile.close();
    }

    // 2. LatestCrash.json에도 같은 내용 덮어쓰기 (가장 최근 크래시 정보 확인용)
    std::ofstream LatestMetaFile(Info.LatestMetaPath);
    if (LatestMetaFile.is_open())
    {
        LatestMetaFile << JsonString;
        LatestMetaFile.close();
    }
}

int ReportCrash(EXCEPTION_POINTERS* ExceptionInfo)
{
    FCrashReportInfo Info = MakeCrashReportInfo(ExceptionInfo);

    WriteCrashDump(ExceptionInfo, Info);
    WriteCrashLog(ExceptionInfo, Info);
    WriteCrashMetadata(ExceptionInfo, Info);

	 ShowCrashMessageBox(Info);

    return EXCEPTION_EXECUTE_HANDLER;
}
FCrashReportInfo MakeCrashReportInfo(EXCEPTION_POINTERS* ExceptionInfo)
{
    FCrashReportInfo Info;

    // 1. Saves/Dump 생성
    FPaths::CreateDir(FPaths::DumpDir());

    // 시간 정보 가져오기
    time_t Now = time(nullptr);
    tm LocalTime;
    localtime_s(&LocalTime, &Now);

    // 2. 현재 시간으로 CrashId 만들기 (예: 20260520_143012)
    char CrashIdBuffer[64];
    snprintf(CrashIdBuffer, sizeof(CrashIdBuffer), "%04d%02d%02d_%02d%02d%02d",
             LocalTime.tm_year + 1900, LocalTime.tm_mon + 1, LocalTime.tm_mday,
             LocalTime.tm_hour, LocalTime.tm_min, LocalTime.tm_sec);
    Info.CrashId = CrashIdBuffer;

    // 3. 사람이 읽기 좋은 TimeString 만들기 (예: 2026-05-20 14:30:12)
    char TimeStringBuffer[64];
    snprintf(TimeStringBuffer, sizeof(TimeStringBuffer), "%04d-%02d-%02d %02d:%02d:%02d",
             LocalTime.tm_year + 1900, LocalTime.tm_mon + 1, LocalTime.tm_mday,
             LocalTime.tm_hour, LocalTime.tm_min, LocalTime.tm_sec);
    Info.TimeString = TimeStringBuffer;

    // 4. ExceptionCodeString 만들기 (예: 0xC0000005)
    Info.ExceptionCode = ExceptionInfo->ExceptionRecord->ExceptionCode;
    char ExceptionCodeBuffer[32];
    snprintf(ExceptionCodeBuffer, sizeof(ExceptionCodeBuffer), "0x%08X", Info.ExceptionCode);
    Info.ExceptionCodeString = ExceptionCodeBuffer;

    // 5. CrashId 기반으로 파일 경로 만들기
    // (FPaths::Combine이 와이드 문자열을 지원한다고 가정)
    WCHAR WCrashId[64];
    swprintf_s(WCrashId, L"%04d%02d%02d_%02d%02d%02d",
               LocalTime.tm_year + 1900, LocalTime.tm_mon + 1, LocalTime.tm_mday,
               LocalTime.tm_hour, LocalTime.tm_min, LocalTime.tm_sec);

    std::wstring DumpFileName = std::wstring(L"Crash_") + WCrashId + L".dmp";
    std::wstring LogFileName = std::wstring(L"Crash_") + WCrashId + L".txt";
    std::wstring MetaFileName = std::wstring(L"Crash_") + WCrashId + L".json";

    Info.DumpPath = FPaths::Combine(FPaths::DumpDir(), DumpFileName);
    Info.LogPath = FPaths::Combine(FPaths::DumpDir(), LogFileName);
    Info.MetaPath = FPaths::Combine(FPaths::DumpDir(), MetaFileName);
    Info.LatestMetaPath = FPaths::Combine(FPaths::DumpDir(), L"LatestCrash.json");

    return Info;
}

void ShowCrashMessageBox(const FCrashReportInfo& Info)
{
    WCHAR Message[MAX_PATH + 128];
    swprintf_s(
        Message,
        L"크래시 리포트가 저장되었습니다.\n\n%s",
        Info.DumpPath.c_str());

    MessageBoxW(nullptr, Message, L"Crash", MB_OK | MB_ICONERROR);
}

bool LoadLatestCrashReportInfo(FCrashReportInfo& OutInfo)
{
    const std::wstring LatestMetaPath = FPaths::Combine(FPaths::DumpDir(), L"LatestCrash.json");
    std::ifstream MetaFile(LatestMetaPath);
    if (!MetaFile.is_open())
    {
        return false;
    }

    std::string Content((std::istreambuf_iterator<char>(MetaFile)), std::istreambuf_iterator<char>());
    json::JSON Root = json::JSON::Load(Content);
    if (Root.JSONType() != json::JSON::Class::Object)
    {
        return false;
    }

    OutInfo = FCrashReportInfo();
    OutInfo.LatestMetaPath = LatestMetaPath;
    OutInfo.CrashId = Root["CrashId"].ToString();
    OutInfo.TimeString = Root["Time"].ToString();
    OutInfo.ExceptionCodeString = Root["ExceptionCode"].ToString();
    OutInfo.DumpPath = FPaths::ToAbsolute(FPaths::ToWide(Root["DumpPath"].ToString()));
    OutInfo.LogPath = FPaths::ToAbsolute(FPaths::ToWide(Root["LogPath"].ToString()));
    OutInfo.MetaPath = FPaths::ToAbsolute(FPaths::ToWide(Root["MetaPath"].ToString()));
    return !OutInfo.CrashId.empty();
}
