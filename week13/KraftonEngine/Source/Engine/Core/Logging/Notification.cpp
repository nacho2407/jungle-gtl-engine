#include "Notification.h"

void FNotificationManager::AddNotification(const FString& Message, ENotificationType Type, float Duration)
{
	std::lock_guard<std::mutex> Lock(Mutex);
	Notifications.push_back({ Message, Type, Duration, 0.0f });
}

void FNotificationManager::Tick(float DeltaTime)
{
	std::lock_guard<std::mutex> Lock(Mutex);

	for (auto It = Notifications.begin(); It != Notifications.end(); )
	{
		It->ElapsedTime += DeltaTime;
		if (It->ElapsedTime >= It->Duration)
		{
			It = Notifications.erase(It);
		}
		else
		{
			++It;
		}
	}
}
