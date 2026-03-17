#pragma once

// �ܼ� â �׸���
void ShowImGuiDemoConsole(bool* p_open);

// �����ε�: �ֿܼ� �α� �߰�
void ShowImGuiDemoConsole(const char* fmt, ...);

// UE_LOG ��ũ��
#define UE_LOG(fmt, ...) ShowImGuiDemoConsole("[LOG] " fmt, ##__VA_ARGS__)