// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "BillboardComponent.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"

// USubUVComponent 컴포넌트이다.
class USubUVComponent : public UBillboardComponent
{
public:
    DECLARE_CLASS(USubUVComponent, UBillboardComponent)

    USubUVComponent();
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
    void SetFrameRate(float InFPS) { PlayRate = InFPS; }
    void SetLoop(bool bInLoop) { bLoop = bInLoop; }
    bool IsLoop() const { return bLoop; }
    bool IsFinished() const { return !bLoop && bIsExecute; }
    void Play()
    {
        FrameIndex = 0;
        TimeAccumulator = 0.0f;
        bIsExecute = false;
    } // 처음부터 다시 재생

    // Sprite Size(Width/Height)는 UBillboardComponent로 끌어올림 — 상속받아 사용.

    // --- Property / Serialization ---
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void Serialize(FArchive& Ar) override;
    void PostDuplicate() override;

    FPrimitiveSceneProxy* CreateSceneProxy() override;
    void UpdateWorldAABB() const override;

protected:
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
    FName ParticleName;
    FParticleResource* CachedParticle = nullptr; // ResourceManager 소유, 여기선 참조만

    uint32 FrameIndex = 0;
    float PlayRate = 30.0f; // 초당 프레임 수
    float TimeAccumulator = 0.0f;

    bool bLoop = true;
    bool bIsExecute = false;
};
