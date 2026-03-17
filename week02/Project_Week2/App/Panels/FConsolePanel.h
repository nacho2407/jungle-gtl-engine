#pragma once

// 콘솔 창 그리기
void ShowImGuiDemoConsole(bool* p_open);

// 오버로드: 콘솔에 로그 추가
void ShowImGuiDemoConsole(const char* fmt, ...);

// UE_LOG 매크로
#define UE_LOG(fmt, ...) ShowImGuiDemoConsole("[LOG] " fmt, ##__VA_ARGS__)