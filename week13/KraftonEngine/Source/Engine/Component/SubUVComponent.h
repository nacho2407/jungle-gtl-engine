#pragma once

#include "BillboardComponent.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"

class UMaterial;

class USubUVComponent : public UBillboardComponent
{
public:
	DECLARE_CLASS(USubUVComponent, UBillboardComponent)
	static void RegisterProperties(UClass* Class);

	USubUVComponent();
	~USubUVComponent() override;

	// --- Particle Resource ---
	// FName 키로 ResourceManager에서 FParticleResource*를 찾아 캐싱
	void SetParticle(const FName& InParticleName);
	const FParticleResource* GetParticle() const { return CachedParticle; }
	const FName& GetParticleName() const { return ParticleName; }
	UMaterial* GetSubUVMaterial() const { return SubUVMaterial; }

	// --- SubUV Frame ---
	void SetFrameIndex(uint32 InIndex) { FrameIndex = static_cast<int32>(InIndex); }
	uint32 GetFrameIndex() const { return static_cast<uint32>(FrameIndex); }

	// --- Playback ---
	void SetFrameRate(float InFPS) { PlayRate = InFPS; }
	void SetLoop(bool bInLoop) { bLoop = bInLoop; }
	bool IsLoop()     const { return bLoop; }
	bool IsFinished() const { return !bLoop && bIsExecute; }
	void Play() { FrameIndex = 0; TimeAccumulator = 0.0f; bIsExecute = false; } // 처음부터 다시 재생

	// --- Property / Serialization ---
	bool ShouldExposeProperty(const FProperty& Property) const override;
	void PostEditProperty(const char* PropertyName) override;

	void PostDuplicate() override;

	FPrimitiveSceneProxy* CreateSceneProxy() override;
	void UpdateWorldAABB() const override;

protected:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
private:
	void RebuildSubUVMaterial();

	UPROPERTY(Edit, Save, Category="Particle", DisplayName="Particle", AssetType="Particle")
	FName ParticleName;
	FParticleResource* CachedParticle = nullptr; // ResourceManager 소유, 여기선 참조만
	UMaterial* SubUVMaterial = nullptr;           // Particle SRV를 래핑하는 경량 머티리얼

	UPROPERTY(Save, Category="Particle", DisplayName="Frame Index", Min=0.0f, Max=100000.0f, Speed=1.0f)
	int32 FrameIndex = 0;
	UPROPERTY(Edit, Save, Category="Particle", DisplayName="Play Rate", Min=1.0f, Max=120.0f, Speed=1.0f)
	float  PlayRate = 30.0f; // 초당 프레임 수
	float  TimeAccumulator = 0.0f;

	UPROPERTY(Edit, Save, Category="Particle", DisplayName="bLoop")
	bool bLoop = true;
	bool bIsExecute = false;
};
