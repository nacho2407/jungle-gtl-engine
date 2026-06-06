#include "AnimNotify_PlaySound.h"

#include "Audio/AudioManager.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Core/Logging/Log.h"
#include "GameFramework/Pawn/Character.h"

namespace
{
	// 이미 LoadAudio 호출 완료한 path 캐시. AudioManager 의 내부 Audios map 에 직접 접근 못 하므로
	// notify 측에서 한 번만 load 보장 (LoadAudio 매번 호출 시 release+reload 비용 회피).
	// 프로세스 lifetime 동안 누적, 캐시 무효화 필요 시 process restart.
	static TSet<FString> GLoadedPlaySoundPaths;
}

void UAnimNotify_PlaySound::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* /*Anim*/)
{
	if (SoundPath.empty()) return;

	if (SoundPath == "Foot1.mp3" || SoundPath == "player.footstep.default")
	{
		if (MeshComp)
		{
			if (ACharacter* Character = Cast<ACharacter>(MeshComp->GetOwner()))
			{
				if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
				{
					if (Movement->IsSprinting())
					{
						return;
					}
				}
			}
		}

		const FVector Position = MeshComp ? MeshComp->GetWorldLocation() : FVector::ZeroVector;
		FAudioManager::Get().PlayEventAt("player.footstep.default", Position);
		return;
	}

	// 캐시 key — path 자체. "AnimNotify:" prefix 로 게임 측 pre-loaded key 들과 namespace 분리.
	const FString Key = FString("AnimNotify:") + SoundPath;

	if (GLoadedPlaySoundPaths.find(SoundPath) == GLoadedPlaySoundPaths.end())
	{
		if (FAudioManager::Get().LoadAudio(Key, SoundPath, /*bLoop=*/false))
		{
			GLoadedPlaySoundPaths.insert(SoundPath);
		}
		else
		{
			UE_LOG("[AnimNotify_PlaySound] LoadAudio failed: %s", SoundPath.c_str());
			return;
		}
	}

	FAudioManager::Get().PlayAudio(Key, Volume);
}
