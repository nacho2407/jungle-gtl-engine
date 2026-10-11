#pragma once

#include "GameFramework/AActor.h"
#include "Object/FName.h"

#include "Source/Engine/GameFramework/Actor/PlayerStart.generated.h"

class USceneComponent;

/**
 * @brief 기본 player pawn spawn 위치 actor
 *
 * @details GameplayPreset.DefaultPlayerStartTag와 StartTag가 일치하는 actor가 기본 pawn 위치로 사용됩니다
 */
UCLASS()
class APlayerStart : public AActor
{
public:
	GENERATED_BODY()
	APlayerStart() = default;
	~APlayerStart() override = default;

	/**
	 * @brief 기본 scene root component를 생성합니다
	 */
	void InitDefaultComponents();

	void BeginPlay() override;
	void PostDuplicate() override;

	FName GetStartTag() const { return StartTag; }
	void SetStartTag(const FName& InStartTag) { StartTag = InStartTag; }

private:
	USceneComponent* RootSceneComponent = nullptr;

	UPROPERTY(Edit, Save, Category="PlayerStart", DisplayName="Start Tag")
	FName StartTag = FName("Default");
};
