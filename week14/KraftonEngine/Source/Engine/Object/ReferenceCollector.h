#pragma once
#include "Core/Types/CoreTypes.h" // TArray가 정의된 곳

class UObject;
// ------------------------------------------------------------------
// GC 참조 수집 인터페이스
// ------------------------------------------------------------------
class FReferenceCollector
{
public:
	~FReferenceCollector() = default;

	void AddReferencedObject(UObject* Object);
	UObject* Pop();

private:
	TArray<UObject*> MarkStack;
};