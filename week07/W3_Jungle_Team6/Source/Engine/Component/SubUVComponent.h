#pragma once

#include "PrimitiveComponent.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"

// SubUV 파티클 스프라이트를 월드 공간에 빌보드로 렌더링하는 컴포넌트.
// PrimitiveComponent를 상속받아 RenderCollector에 자동으로 감지됩니다.
// MeshBuffer를 사용하지 않으며, SubUVBatcher가 드로우콜을 처리합니다.
//
// 사용 예:
//   USubUVComponent* Comp = Actor->AddComponent<USubUVComponent>();
//   Comp->SetParticle(FName("Explosion"));
//   Comp->SetFrameIndex(CurrentFrame);
//   Comp->SetSpriteSize(2.0f, 2.0f);
class USubUVComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(USubUVComponent, UPrimitiveComponent)

	USubUVComponent() = default;
	~USubUVComponent() override = default;

	// --- Particle Resource ---
	// FName 키로 ResourceManager에서 FParticleResource*를 찾아 캐싱
	void SetParticle(const FName& InParticleName);
	const FParticleResource* GetParticle() const { return CachedParticle; }
	const FName& GetParticleName() const { return ParticleName; }

	// --- SubUV Frame ---
	void SetFrameIndex(uint32 InIndex) { FrameIndex = InIndex; }
	uint32 GetFrameIndex() const { return FrameIndex; }

	// --- Playback ---
	void SetFrameRate(float InFPS) { FrameRate = InFPS; }

	// --- Sprite Size (월드 공간 크기) ---
	void SetSpriteSize(float InWidth, float InHeight) { Width = InWidth; Height = InHeight; }
	float GetWidth()  const { return Width; }
	float GetHeight() const { return Height; }

	// --- Property / Serialization ---
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	// --- PrimitiveComponent 인터페이스 ---
	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_SubUV;

protected:
	void TickComponent(float DeltaTime) override;

private:
	FName ParticleName;
	FParticleResource* CachedParticle = nullptr; // ResourceManager 소유, 여기선 참조만

	uint32 FrameIndex       = 0;
	float  Width            = 1.0f;
	float  Height           = 1.0f;
	float  FrameRate        = 30.0f; // 초당 프레임 수
	float  TimeAccumulator  = 0.0f;
};
