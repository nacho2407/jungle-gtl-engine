#include "Engine/Core/CrashTest.h"

#include "Object/Object.h"

#include <Windows.h>

bool FCrashTest::bRandomObjectDeletionEnabled = false;
int32 FCrashTest::RandomObjectDeletionsPerFrame = 1;

void FCrashTest::CauseCrash()
{
	// nullptr 쓰기로 Access Violation을 안정적으로 발생시킵니다.
	volatile int* Ptr = nullptr;
	*Ptr = 123;
}

void FCrashTest::RaiseTestException()
{
	// Windows SEH 경로가 덤프를 남기는지 확인하기 위한 사용자 정의 예외입니다.
	RaiseException(0xE0000001, EXCEPTION_NONCONTINUABLE, 0, nullptr);
}

void FCrashTest::EnableRandomObjectDeletion(bool bEnable, int32 DeletionsPerFrame)
{
	bRandomObjectDeletionEnabled = bEnable;

	if (DeletionsPerFrame < 1)
	{
		DeletionsPerFrame = 1;
	}

	RandomObjectDeletionsPerFrame = DeletionsPerFrame;
}

// 콘솔에서 crash dangle을 확정한 경우에만 동작합니다.
void FCrashTest::TickRandomObjectDeletion()
{
	if (!bRandomObjectDeletionEnabled)
	{
		return;
	}

	for (int32 Index = 0; Index < RandomObjectDeletionsPerFrame; ++Index)
	{
		// 일반 소유권 경로를 깨뜨려 실제 dangling 참조 크래시를 재현하는 fault injection입니다.
		UObject* Obj = UObjectManager::Get().GetRandomObject();
		if (!Obj)
		{
			continue;
		}

		UObjectManager::Get().DestroyObject(Obj);
	}
}
