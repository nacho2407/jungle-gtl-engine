#pragma once
#include "CoreMinimal.h"
#include "Math/Vector.h"

//Json을 담아두는곳
class ENGINE_API FArchive
{
public:
	FArchive(bool bInSaving);
	~FArchive();
	bool IsSaving() const { return bSaving; }
	bool IsLoading() const { return !bSaving; }
	// 기본 타입 직렬화
	void Serialize(const FString& Key, FString& Value);
	void Serialize(const FString& Key, uint32& Value);
	void Serialize(const FString& Key, bool& Value);
	void Serialize(const FString& Key, FVector& Value);
	void Serialize(const FString& Key, FVector4& Value);
	// 배열
	void SerializeUIntArray(const FString& Key, TArray<uint32>& Values);

	// 키 존재 여부
	bool Contains(const FString& Key) const;
	void* GetRawJson();
private:
	bool bSaving;
	void* JsonData;// nlohmann::json* — 헤더에 json 노출 안 함
};