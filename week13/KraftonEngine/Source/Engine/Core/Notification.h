#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include <mutex>
#include <chrono>

// ============================================================
// ENotificationType — 알림 종류 (색상 구분용)
// ============================================================
enum class ENotificationType : uint8
{
	Info,
	Success,
	Error,
};

// ============================================================
// FNotification — 단일 알림 데이터
// ============================================================
struct FNotification
{
	FString Message;
	ENotificationType Type = ENotificationType::Info;
	float Duration = 3.0f;         // 표시 시간(초)
	float ElapsedTime = 0.0f;      // 경과 시간
};

// ============================================================
// FNotificationManager — Engine 레이어 알림 큐 싱글턴
//
// 어디서든 AddNotification()으로 메시지를 던지면
// Editor 레이어에서 매 프레임 읽어 ImGui 토스트로 렌더링한다.
// ============================================================
class FNotificationManager : public TSingleton<FNotificationManager>
{
	friend class TSingleton<FNotificationManager>;

public:
	void AddNotification(const FString& Message, ENotificationType Type = ENotificationType::Info, float Duration = 3.0f);

	// Editor 레이어에서 매 프레임 호출 — 만료된 알림 제거 + 현재 목록 반환
	const TArray<FNotification>& GetNotifications() const { return Notifications; }
	void Tick(float DeltaTime);

private:
	FNotificationManager() = default;

	std::mutex Mutex;
	TArray<FNotification> Notifications;
};
