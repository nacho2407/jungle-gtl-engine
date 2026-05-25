#include "ParticleSystem.h"

#include "ParticleEmitter.h"
#include "Serialization/Archive.h"

void UParticleSystem::Serialize(FArchive& Ar)
{
    // Version 1: 시스템/썸네일/LOD 정책 필드 추가. 옛 파일(Version 0)은 LODDistances 만 읽고
    // 나머지 시스템 필드는 디폴트값을 유지한다.
    int32 Version = 1;
    Ar << Version;
    Ar << LODDistances;

    if (Version >= 1)
    {
        Ar << SystemUpdateMode;
        Ar << UpdateTimeFPS;
        Ar << WarmupTime;
        Ar << WarmupTickRate;
        Ar << bOrientZAxisTowardCamera;
        Ar << SecondsBeforeInactive;
        Ar << ThumbnailWarmup;
        Ar << bUseRealtimeThumbnail;
        Ar << LODDistanceCheckTime;
        Ar << LODMethod;
    }

    if (Ar.IsLoading())
    {
        Emitters.clear();
    }

    int32 EmitterCount = static_cast<int32>(Emitters.size());
    Ar << EmitterCount;

    if (Ar.IsSaving())
    {
        for (UParticleEmitter* Emitter : Emitters)
        {
            bool bValid = (Emitter != nullptr);
            Ar << bValid;
            if (bValid)
            {
                Emitter->Serialize(Ar);
            }
        }
    }
    else if (Ar.IsLoading())
    {
        for (int32 Index = 0; Index < EmitterCount; ++Index)
        {
            bool bValid = false;
            Ar << bValid;
            if (!bValid)
            {
                Emitters.push_back(nullptr);
                continue;
            }

            UParticleEmitter* Emitter = UObjectManager::Get().CreateObject<UParticleEmitter>(this);
            Emitter->Serialize(Ar);
            Emitters.push_back(Emitter);
        }
    }
}
