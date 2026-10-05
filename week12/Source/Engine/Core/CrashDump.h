#pragma once

#include <Windows.h>

// SEH 필터 함수: __except() 안에서 GetExceptionInformation()과 함께 사용
// 크래시 발생 시 실행 파일 옆에 .dmp 파일을 생성합니다.
LONG WINAPI WriteCrashDump(EXCEPTION_POINTERS* ExceptionInfo);
void WriteCrashLog(EXCEPTION_POINTERS* ExceptionInfo);
int ReportCrash(EXCEPTION_POINTERS* ExceptionInfo);

struct FCrashHandler
{
	// 프로세스 전역 Unhandled Exception Filter를 등록합니다.
	static void Initialize();

	// SEH에서 전달받은 예외 정보로 덤프와 텍스트 로그를 모두 남깁니다.
	static LONG WINAPI HandleException(EXCEPTION_POINTERS* ExceptionInfo);
	static void WriteCrashDump(EXCEPTION_POINTERS* ExceptionInfo);
	static void WriteCrashLog(EXCEPTION_POINTERS* ExceptionInfo);
};
