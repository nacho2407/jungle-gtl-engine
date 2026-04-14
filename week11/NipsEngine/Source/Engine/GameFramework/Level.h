#pragma once
#include "Object/Object.h"
#include "GameFramework/AActor.h"

class ULevel : public UObject
{
public:
	DECLARE_CLASS(ULevel, UObject)

	ULevel() = default;
	virtual ~ULevel() override;

	virtual void PostDuplicate(UObject* Original) override;

	// 프로퍼티 시스템 — UObject 에서 상속
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override {}
	void PostEditProperty(const char* PropertyName) override {}

	void AddActor(AActor* Actor) { Actors.push_back(Actor); }
	void RemoveActor(AActor* Actor) {
		auto it = std::find(Actors.begin(), Actors.end(), Actor);
		if (it != Actors.end()) Actors.erase(it);
	}

	const TArray<AActor*>& GetActors() const { return Actors; }

	void BeginPlay();
	void TickEditor(float DeltaTime);   // bTickInEditor == true 인 액터만 틱
	void TickGame(float DeltaTime);     // 활성화된 모든 액터를 틱 (PIE / Game)
	void EndPlay(EEndPlayReason::Type EndPlayReason);

private:
	TArray<AActor*> Actors;
};

