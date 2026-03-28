#pragma once
#include "CoreMinimal.h"
#include <algorithm>
#include <memory>

class UObject;
class UClass;

class ENGINE_API ObjectManager
{
public:
	ObjectManager();
	~ObjectManager();

	// UClass*를 직접 넘기는 버전
	UObject* SpawnObject(
		UClass* InClass,
		UObject* InOuter = nullptr,
		const FString& InName = "None");

	// PendingKill 마킹 후 delete
	// 소멸자에서 GUObjectArray 슬롯이 자동으로 nullptr 처리됨
	void ReleaseObject(UObject* obj);

	// GUObjectArray의 nullptr 슬롯을 제거하고 InternalIndex 재조정
	void FlushKilledObjects();

};