#pragma once

#include <Windows.h>
#include <string>
struct FCrashReportInfo
{
    // 크래시 고유 식별자 (예: UUID)
    std::string CrashId;             // "20260520_143012"
    std::string TimeString;      // "2026-05-20 14:30:12"
    std::string ExceptionCodeString; // "0xC0000005"

    // 파일 경로들
    std::wstring DumpPath;
    std::wstring LogPath;
    std::wstring MetaPath;
    std::wstring LatestMetaPath;

    // OS 예외 코드 (예: Windows의 0xC0000005)
    uint32_t ExceptionCode;

    // 기본 생성자 (초기화)
    FCrashReportInfo()
        : ExceptionCode(0)
    {
    }
};
// SEH 필터 함수: __except() 안에서 GetExceptionInformation()과 함께 사용
// 크래시 발생 시 실행 파일 옆에 .dmp 파일을 생성합니다.
LONG WINAPI WriteCrashDump(EXCEPTION_POINTERS* ExceptionInfo, const FCrashReportInfo& Info);
void WriteCrashLog(EXCEPTION_POINTERS* ExceptionInfo, const FCrashReportInfo& Info);
void WriteCrashMetadata(EXCEPTION_POINTERS* ExceptionInfo, const FCrashReportInfo& Info);
int ReportCrash(EXCEPTION_POINTERS* ExceptionInfo);
FCrashReportInfo MakeCrashReportInfo(EXCEPTION_POINTERS* ExceptionInfo);
void ShowCrashMessageBox(const FCrashReportInfo& Info);
bool LoadLatestCrashReportInfo(FCrashReportInfo& OutInfo);
