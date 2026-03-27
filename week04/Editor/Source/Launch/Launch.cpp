#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <fstream>
#include <iomanip>
#pragma comment(lib, "dbghelp.lib")

#include "Launch.h"
#include "Launch/EditorEngineLoop.h"

void WriteCrashLog(EXCEPTION_POINTERS* ExceptionInfo)
{
    HANDLE process = GetCurrentProcess();

    // 1. 심볼 핸들러 초기화 (PDB 파일에서 정보를 읽어오기 위한 준비)
    SymInitialize(process, NULL, TRUE);

    // 2. 로그 파일 열기 (Append 모드)
    std::ofstream logFile("CrashLog.txt", std::ios::app);
    logFile << "========================================\n";
    logFile << "[Engine Crash Report]\n";
    logFile << "Exception Code: 0x" << std::hex << ExceptionInfo->ExceptionRecord->ExceptionCode
            << "\n";
    logFile << "Call Stack:\n";

    // 3. 콜스택 캡처 (최대 62프레임까지 역추적)
    const int maxFrames = 62;
    void*     stack[maxFrames];
    WORD      frames = CaptureStackBackTrace(0, maxFrames, stack, NULL);

    // 4. 주소를 함수 이름과 라인 번호로 변환하기 위한 구조체 세팅
    char         symbolBuffer[sizeof(SYMBOL_INFO) + 256 * sizeof(char)];
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)symbolBuffer;
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    IMAGEHLP_LINE64 line;
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    DWORD displacement;

    // 5. 캡처된 프레임을 순회하며 정보 기록
    for (WORD i = 0; i < frames; i++)
    {
        DWORD64 address = (DWORD64)(stack[i]);
        SymFromAddr(process, address, 0, symbol); // 함수 이름 추출

        // 파일 이름 및 라인 번호 추출 시도
        if (SymGetLineFromAddr64(process, address, &displacement, &line))
        {
            logFile << "[" << i << "] " << symbol->Name << " (" << line.FileName << " : "
                    << std::dec << line.LineNumber << ")\n";
        }
        else
        {
            // PDB 정보가 부족하여 라인 번호를 찾지 못한 경우 함수 이름과 메모리 주소만 출력
            logFile << "[" << i << "] " << symbol->Name << " (Address: 0x" << std::hex << address
                    << ")\n";
        }
    }

    logFile << "========================================\n\n";
    logFile.close();

    // 6. 심볼 핸들러 정리
    SymCleanup(process);
}

int ReportCrash(EXCEPTION_POINTERS* ExceptionInfo)
{
    WriteCrashLog(ExceptionInfo);

    // 디버그 출력창에 에러 코드 출력 (선택 사항)
    DWORD   ExceptionCode = ExceptionInfo->ExceptionRecord->ExceptionCode;
    wchar_t ErrorMsg[256];
    wsprintf(ErrorMsg, L"Fatal Error: Exception Code 0x%08X. Check CrashLog.txt", ExceptionCode);
    OutputDebugStringW(ErrorMsg);

    // return EXCEPTION_CONTINUE_SEARCH;
    return EXCEPTION_EXECUTE_HANDLER;

}

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
        
        EditorEngineLoop.ShutDown();
        
        return ExitCode;
    }
}

int Launch(HINSTANCE HInstance, int NCmdShow)
{
    int ExitCode = -1;

    __try
    {
        ExitCode = GuardedMain(HInstance, NCmdShow);
    }
    __except (ReportCrash(GetExceptionInformation()))
    {
        MessageBoxW(NULL, L"엔진에 치명적인 오류가 발생하여 종료됩니다. 크래시 로그를 확인하세요.",
                    L"Engine Crash", MB_ICONERROR | MB_OK);

        ExitCode = -1;
    }
    return GuardedMain(HInstance, NCmdShow);
}