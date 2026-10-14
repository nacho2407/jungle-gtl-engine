#pragma once

#include "GameFramework/AActor.h"
#include "Object/FName.h"
#include "Math/Vector.h"

#include "Source/Engine/GameFramework/Actor/TriggerVolumeBase.generated.h"
class UBoxComponent;
class UPrimitiveComponent;
class APawn;
struct FHitResult;

/**
 * @brief Possessed Pawn overlap을 Lua EventBus로 전달하는 trigger volume
 *
 * @details TriggerTag는 Lua 쪽에서 트리거 종류를 구분하는 식별자로 전달됩니다
 */
UCLASS()
class ATriggerVolumeBase : public AActor
{
public:
	GENERATED_BODY()
	ATriggerVolumeBase() = default;
	~ATriggerVolumeBase() override = default;

	void BeginPlay() override;

	/**
	 * @brief 기본 BoxComponent를 Trigger 셋업과 함께 추가합니다
	 *
	 * @param Extent 생성할 trigger box extent
	 */
	void InitDefaultComponents(const FVector& Extent = FVector(1.0f, 1.0f, 1.0f));
	void PostDuplicate() override;

	// 서브클래스 추가 동작 hook
	virtual void OnPossessedPawnEntered(APawn* Pawn) {}
	virtual void OnPossessedPawnExited(APawn* Pawn) {}

	UBoxComponent* GetTriggerBox() const { return TriggerBox; }

	// Lua EventBus가 트리거 종류를 구분할 때 사용하는 식별자입니다.
	FName GetTriggerTag() const { return TriggerTag; }
	void SetTriggerTag(const FName& InTag) { TriggerTag = InTag; }

protected:
	// 델리게이트 시그니처
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	void HandleEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	UBoxComponent* TriggerBox = nullptr;

	UPROPERTY(Edit, Save, Category="Trigger", DisplayName="TriggerTag")
	FName TriggerTag;  // 직렬화 - 디자이너가 씬에서 식별자를 지정

	UPROPERTY(Edit, Save, Category="Trigger", DisplayName="Only Possessed Pawn")
	bool bOnlyPossessedPawn = true;
};
