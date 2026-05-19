#pragma once

#include "AnimNotify.h"
#include "Core/CoreTypes.h"

// Phase 7 데모용 구체 Notify — 트리거 시 Console/VS Output 에 메시지 로깅.
//   - Message 는 시퀀스/임포터가 채워 넣음.
//   - 시퀀스(UAnimDataModel) 가 인스턴스를 소유 (Outer 체인).

#include "Source/Engine/Animation/AnimNotify_LogMessage.generated.h"

UCLASS()
class UAnimNotify_LogMessage : public UAnimNotify
{
public:
	GENERATED_BODY()
	UAnimNotify_LogMessage() = default;
	~UAnimNotify_LogMessage() override = default;

	UPROPERTY(Edit, Save, Category="Notify", DisplayName="Message")
	FString Message = "LogMessage";

	void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim) override;
};
