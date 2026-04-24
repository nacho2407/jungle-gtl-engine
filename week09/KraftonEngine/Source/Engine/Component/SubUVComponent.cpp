// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "SubUVComponent.h"
#include "Object/ObjectFactory.h"

#include <cstring>
#include "Render/Resources/Buffers/MeshBufferManager.h"
#include "Resource/ResourceManager.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Component/CameraComponent.h"
#include "Render/Scene/Proxies/Primitive/SubUVSceneProxy.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(USubUVComponent, UBillboardComponent)

FPrimitiveSceneProxy* USubUVComponent::CreateSceneProxy()
{
    return new FSubUVSceneProxy(this);
}

void USubUVComponent::Serialize(FArchive& Ar)
{
    /*
        공통 width/height 직렬화를 위해 부모 빌보드 직렬화를 먼저 호출합니다.
        SubUV는 별도 particle 리소스를 사용하므로 particle 전용 값은 아래에서 추가로 저장합니다.
    */
    UBillboardComponent::Serialize(Ar);

    Ar << ParticleName;
    Ar << FrameIndex;
    Ar << PlayRate;
    Ar << bLoop;
}

void USubUVComponent::PostDuplicate()
{
    UBillboardComponent::PostDuplicate();
    // particle 리소스를 다시 바인딩합니다.
    SetParticle(ParticleName);
}

USubUVComponent::USubUVComponent()
{
    SetVisibility(false);
}

void USubUVComponent::SetParticle(const FName& InParticleName)
{
    ParticleName = InParticleName;
    CachedParticle = FResourceManager::Get().FindParticle(InParticleName);
}

void USubUVComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    /*
        SubUV는 빌보드의 texture 속성을 노출하지 않습니다.
        primitive 공통 속성과 SubUV 전용 속성만 에디터에 등록합니다.
    */
    UPrimitiveComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Particle", EPropertyType::Name, &ParticleName });
    OutProps.push_back({ "Width", EPropertyType::Float, &Width, 0.1f, 100.0f, 0.1f });
    OutProps.push_back({ "Height", EPropertyType::Float, &Height, 0.1f, 100.0f, 0.1f });
    OutProps.push_back({ "Play Rate", EPropertyType::Float, &PlayRate, 1.0f, 120.0f, 1.0f });
    OutProps.push_back({ "bLoop", EPropertyType::Bool, &bLoop });
}

void USubUVComponent::PostEditProperty(const char* PropertyName)
{
    /*
        빌보드 texture 처리 경로를 건너뛰기 위해 primitive 편집 처리를 직접 호출합니다.
    */
    UPrimitiveComponent::PostEditProperty(PropertyName);

    if (strcmp(PropertyName, "Particle") == 0)
    {
        SetParticle(ParticleName);
        // particle 교체는 UV 그리드와 texture를 바꾸므로 메시 단계까지 dirty 처리합니다.
        MarkProxyDirty(ESceneProxyDirtyFlag::Mesh);
    }
    else if (strcmp(PropertyName, "Width") == 0 || strcmp(PropertyName, "Height") == 0)
    {
        MarkProxyDirty(ESceneProxyDirtyFlag::Transform);
        MarkWorldBoundsDirty();
    }
}

void USubUVComponent::UpdateWorldAABB() const
{
    FVector LExt = { 0.01f, 0.5f, 0.5f };

    float NewEx = std::abs(CachedWorldMatrix.M[0][0]) * LExt.X +
                  std::abs(CachedWorldMatrix.M[1][0]) * LExt.Y +
                  std::abs(CachedWorldMatrix.M[2][0]) * LExt.Z;

    float NewEy = std::abs(CachedWorldMatrix.M[0][1]) * LExt.X +
                  std::abs(CachedWorldMatrix.M[1][1]) * LExt.Y +
                  std::abs(CachedWorldMatrix.M[2][1]) * LExt.Z;

    float NewEz = std::abs(CachedWorldMatrix.M[0][2]) * LExt.X +
                  std::abs(CachedWorldMatrix.M[1][2]) * LExt.Y +
                  std::abs(CachedWorldMatrix.M[2][2]) * LExt.Z;

    FVector WorldCenter = GetWorldLocation();

    WorldAABBMinLocation = WorldCenter - FVector(NewEx, NewEy, NewEz);
    WorldAABBMaxLocation = WorldCenter + FVector(NewEx, NewEy, NewEz);
}

void USubUVComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    UBillboardComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!CachedParticle)
        return;
    if (!bLoop && bIsExecute)
        return;

    const uint32 TotalFrames = CachedParticle->Columns * CachedParticle->Rows;
    if (TotalFrames == 0)
        return;

    TimeAccumulator += DeltaTime;
    const float FrameDuration = 1.0f / PlayRate;
    while (TimeAccumulator >= FrameDuration)
    {
        TimeAccumulator -= FrameDuration;

        if (bLoop)
        {
            bIsExecute = false;
            FrameIndex = (FrameIndex + 1) % TotalFrames; // 臾댄븳 諛섎났
        }
        else
        {
            if (FrameIndex < TotalFrames - 1)
            {
                FrameIndex++;
            }
            else
            {
                bIsExecute = true; // 마지막 프레임 도달 후 완료
                TimeAccumulator = 0.0f;
                break;
            }
        }
    }
}
