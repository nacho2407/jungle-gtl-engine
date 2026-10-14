#include "CharacterAnimGraphInstance.h"

#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "GameFramework/AActor.h"
#include "Math/MathUtils.h"
#include "Serialization/Archive.h"

#include <cmath>

void UCharacterAnimGraphInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	// 부모 (UAnimGraphInstance) 가 자산 Version polling + 재컴파일 처리.
	Super::NativeUpdateAnimation(DeltaSeconds);

	Speed      = 0.0f;
	bIsFalling = false;
	if (USkeletalMeshComponent* Comp = GetOwningComponent())
	{
		if (AActor* Owner = Comp->GetOwner())
		{
			if (UCharacterMovementComponent* Move = Owner->GetComponentByClass<UCharacterMovementComponent>())
			{
				Speed      = Move->GetSpeed();
				bIsFalling = Move->IsFalling();
			}
		}
	}
}

void UCharacterAnimGraphInstance::Serialize(FArchive& Ar)
{
	// 부모 직렬화 — GraphAssetPath / DefaultSequencePath 라운드트립.
	Super::Serialize(Ar);
}
