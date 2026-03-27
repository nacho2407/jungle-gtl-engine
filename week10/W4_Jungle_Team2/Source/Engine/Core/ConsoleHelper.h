#pragma once
#include <Windows.h>
#include <iostream>
#include <cstdio>

class ConsoleHelper {
public:
    ConsoleHelper() {
        if (AllocConsole()) {
            freopen_s(&m_fpIn, "CONIN$", "r", stdin);
            freopen_s(&m_fpOut, "CONOUT$", "w", stdout);
            freopen_s(&m_fpErr, "CONOUT$", "w", stderr);

            std::ios::sync_with_stdio();
        }
    }

    ~ConsoleHelper() {
        if (m_fpIn)  fclose(m_fpIn);
        if (m_fpOut) fclose(m_fpOut);
        if (m_fpErr) fclose(m_fpErr);

        FreeConsole();
    }

    ConsoleHelper(const ConsoleHelper&) = delete;
    ConsoleHelper& operator=(const ConsoleHelper&) = delete;

private:
    FILE* m_fpIn = nullptr;
    FILE* m_fpOut = nullptr;
    FILE* m_fpErr = nullptr;
};