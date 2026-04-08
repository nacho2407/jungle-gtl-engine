#pragma once
#include "Object/Object.h"
#include "GameFramework/AActor.h"

class ULevel : public UObject
{
public:
	DECLARE_CLASS(ULevel, UObject)

	ULevel() = default;
	virtual ~ULevel() override;

	virtual ULevel* Duplicate() override;
    virtual ULevel* DuplicateSubObjects() override { return this; }

	void AddActor(AActor* Actor) { Actors.push_back(Actor); }
	void RemoveActor(AActor* Actor) {
		auto it = std::find(Actors.begin(), Actors.end(), Actor);
		if (it != Actors.end()) Actors.erase(it);
	}

	const TArray<AActor*>& GetActors() const { return Actors; }
	void BeginPlay();
	void Tick(float DeltaTime);
	void EndPlay();

private:
	TArray<AActor*> Actors;
};

