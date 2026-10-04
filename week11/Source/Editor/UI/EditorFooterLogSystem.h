#pragma once

#include "Core/CoreMinimal.h"

#include <algorithm>

class FEditorFooterLogSystem
{
public:
	void Tick(float DeltaTime)
	{
		for (FLogEntry& Entry : Entries)
		{
			Entry.RemainingSeconds -= DeltaTime;
		}

		Entries.erase(
			std::remove_if(
				Entries.begin(),
				Entries.end(),
				[](const FLogEntry& Entry)
				{
					return Entry.RemainingSeconds <= 0.0f;
				}),
			Entries.end());
	}

	void Push(const FString& InMessage, float InLifetimeSeconds = 5.0f)
	{
		if (InMessage.empty() || InLifetimeSeconds <= 0.0f)
		{
			return;
		}

		Entries.push_back({ InMessage, InLifetimeSeconds });
		if (Entries.size() > 6)
		{
			Entries.erase(Entries.begin());
		}
	}

	TArray<FString> GetActiveMessages() const
	{
		TArray<FString> Messages;
		Messages.reserve(Entries.size());
		for (const FLogEntry& Entry : Entries)
		{
			if (Entry.RemainingSeconds > 0.0f)
			{
				Messages.push_back(Entry.Message);
			}
		}
		return Messages;
	}

private:
	struct FLogEntry
	{
		FString Message;
		float RemainingSeconds = 0.0f;
	};

	TArray<FLogEntry> Entries;
};
