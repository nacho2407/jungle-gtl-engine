#pragma once

#include "GameFramework/World.h"
#include "GameFramework/Level.h"
#include "GameFramework/AActor.h"

template<typename T>
class TActorIterator
{
public:
	// 생성 시 대상 월드를 인자로 받습니다.
	TActorIterator(UWorld* InWorld)
		: CurrentWorld(InWorld)
		, ActorIndex(0)
	{
		if (CurrentWorld && CurrentWorld->GetPersistentLevel())
		{
			// 첫 번째 유효한 액터 위치로 이동합니다.
			FindNextValidActor();
		}
	}

	// 반복자 종료 조건 확인 (bool 변환)
	inline operator bool() const { return CurrentActor != nullptr; }

	// 다음 액터로 이동 (전위 증가 연산자)
	inline TActorIterator& operator++()
	{
		ActorIndex++;
		FindNextValidActor();
		return *this;
	}

	// 현재 액터 포인터 반환
	inline T* operator*() const { return CurrentActor; }
	inline T* operator->() const { return CurrentActor; }

private:
	void FindNextValidActor()
	{
		CurrentActor = nullptr;

		if (!CurrentWorld || !CurrentWorld->GetPersistentLevel()) return;

		const TArray<AActor*>& Actors = CurrentWorld->GetPersistentLevel()->GetActors();

		while (ActorIndex < Actors.size())
		{
			AActor* PotentialActor = Actors[ActorIndex];

			if (PotentialActor != nullptr)
			{
				if (PotentialActor->IsA<T>())
				{
					CurrentActor = static_cast<T*>(PotentialActor);
					return;
				}
			}

			// 유효하지 않으면 다음 인덱스로 이동
			ActorIndex++;
		}
	}

private:
	UWorld* CurrentWorld;
	T* CurrentActor = nullptr;
	int32 ActorIndex;
};