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


class IGCReferenceProvider
{
public:
	virtual ~IGCReferenceProvider() = default;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) = 0;
};

class FGCReferenceRegistry
{
public:
	static void Register(IGCReferenceProvider* Provider);
	static void Unregister(IGCReferenceProvider* Provider);
	static void AddReferencedObjects(FReferenceCollector& Collector);

	//사용법
	/*
	FParticleSystemManager::FParticleSystemManager()
	{
		FGCReferenceRegistry::Register(this);
	}

	FParticleSystemManager::~FParticleSystemManager()
	{
		FGCReferenceRegistry::Unregister(this);
	}
	*/
private:
	static TArray<IGCReferenceProvider*> Providers;
};
