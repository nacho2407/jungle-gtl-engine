#pragma once
#include "CoreMinimal.h"
#include <functional>

class UClass;
class UObject;

// 조건 3: UObject를 생성해주는 Factory 클래스
// UClass에 등록된 팩토리 람다를 통해 인스턴스를 만들고,
// UUID / InternalIndex를 부여한 뒤 GUObjectArray에 등록한다.
//
// 사용 예)
//   UObject*   obj  = FObjectFactory::ConstructObject(UObject::StaticClass());
//   UPrimitive* p   = FObjectFactory::ConstructObject(UPrimitive::StaticClass());
//   USphere*    s   = FObjectFactory::ConstructObject<USphere>();
class ENGINE_API FObjectFactory
{
public:
	// UClass*를 직접 지정하는 버전
	static UObject* ConstructObject(
		UClass* InClass,
		UObject* InOuter = nullptr,
		const FString& InName = "None"
	);

	// 템플릿 버전 — T::StaticClass()를 자동으로 전달
	template<typename T>
	static T* ConstructObject(
		UObject* InOuter = nullptr,
		const FString& InName = "None"
	)
	{
		static_assert(std::is_base_of_v<UObject, T>, "T must derive from UObject");
		return static_cast<T*>(ConstructObject(T::StaticClass(), InOuter, InName));
	}

	static uint32 GetLastUUID();
	static void SetLastUUID(uint32 InUUID);
private:
	static uint32 LastUUID;
	static uint32 GenerateUUID();
};