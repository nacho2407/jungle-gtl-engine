#include "CharacterAnimGraphInstance.h"

#include "Math/MathUtils.h"
#include "Serialization/Archive.h"

#include <cmath>

void UCharacterAnimGraphInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	// 부모 (UAnimGraphInstance) 가 자산 Version polling + 재컴파일 처리.
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Speed 자동 변동 — UCharacterAnimInstance 와 동일 패턴.
	if (bAutoDriveSpeed && AutoPeriodSec > 0.0f)
	{
		ElapsedTime += DeltaSeconds;
		const float Omega = 2.0f * FMath::Pi / AutoPeriodSec;
		Speed = AutoSpeedAmp + AutoSpeedAmp * std::sin(ElapsedTime * Omega);
	}
}

void UCharacterAnimGraphInstance::Serialize(FArchive& Ar)
{
	// 부모 직렬화 — GraphAssetPath / DefaultSequencePath 라운드트립.
	Super::Serialize(Ar);

	Ar << bAutoDriveSpeed;
	Ar << AutoPeriodSec;
	Ar << AutoSpeedAmp;
}
