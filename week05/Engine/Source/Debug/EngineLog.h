#pragma once
#include "CoreMinimal.h"
#include <functional>

using FLogCallback = std::function<void(const char*)>;

class ENGINE_API FEngineLog
{
public:
	static FEngineLog& Get();

	void Log(const char* Format, ...);
	void SetCallback(FLogCallback InCallback);

private:
	FEngineLog() = default;
	FLogCallback Callback;
};

#define UE_LOG(Format, ...) FEngineLog::Get().Log(Format, ##__VA_ARGS__)
