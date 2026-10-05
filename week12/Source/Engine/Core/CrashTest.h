#pragma once

#include "Core/CoreMinimal.h"

struct FCrashTest
{
	// 즉시 재현 가능한 크래시 경로입니다. 콘솔 명령에서 수동 확인 후 호출됩니다.
	static void CauseCrash();
	static void RaiseTestException();

	// 매 프레임 임의 UObject를 삭제해 dangling pointer 계열 문제를 강제로 유도합니다.
	static void EnableRandomObjectDeletion(bool bEnable, int32 DeletionsPerFrame = 1);
	static void TickRandomObjectDeletion();

private:
	static bool bRandomObjectDeletionEnabled;
	static int32 RandomObjectDeletionsPerFrame;
};
