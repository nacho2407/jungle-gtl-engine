// 엔진 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include <Windows.h>
#include <iostream>
#include <cstdio>

// ConsoleHelper는 엔진 영역의 핵심 동작을 담당합니다.
class ConsoleHelper
{
public:
    ConsoleHelper()
    {
        if (AllocConsole())
        {
            freopen_s(&m_fpIn, "CONIN$", "r", stdin);
            freopen_s(&m_fpOut, "CONOUT$", "w", stdout);
            freopen_s(&m_fpErr, "CONOUT$", "w", stderr);

            std::ios::sync_with_stdio();
        }
    }

    ~ConsoleHelper()
    {
        if (m_fpIn)
            fclose(m_fpIn);
        if (m_fpOut)
            fclose(m_fpOut);
        if (m_fpErr)
            fclose(m_fpErr);

        FreeConsole();
    }

    ConsoleHelper(const ConsoleHelper&) = delete;
    ConsoleHelper& operator=(const ConsoleHelper&) = delete;

private:
    FILE* m_fpIn = nullptr;
    FILE* m_fpOut = nullptr;
    FILE* m_fpErr = nullptr;
};
