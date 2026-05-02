#pragma once

#include "GameFramework/AActor.h"
#include "Object/FName.h"
#include "Math/Vector.h"

class UBoxComponent;
class UPrimitiveComponent;
class APawn;
struct FHitResult;

// ============================================================
// ATriggerVolumeBase — Possessed Pawn 진입 시 GameMode에 통지
//
// 기본 동작:
//   1) BoxComponent를 Trigger 채널/Overlap-only로 셋업
//   2) BeginPlay에서 OnComponentBeginOverlap/EndOverlap 바인딩
//   3) Overlap 들어온 액터가 APawn이고 IsPossessed()일 때만 처리
//   4) GameMode->OnPossessedPawnEnteredTrigger(this, Pawn) 호출
//
// 게임모드에서 트리거 종류를 구분할 때는 TriggerTag(FName)로 식별하거나
// 본 클래스를 상속받은 서브클래스로 분기.
// ============================================================
class ATriggerVolumeBase : public AActor
{
public:
	DECLARE_CLASS(ATriggerVolumeBase, AActor)

	ATriggerVolumeBase() = default;
	~ATriggerVolumeBase() override = default;

	void BeginPlay() override;

	// 코드 spawn 시 호출. 기본 BoxComponent를 Trigger 셋업과 함께 추가한다.
	void InitDefaultComponents(const FVector& Extent = FVector(1.0f, 1.0f, 1.0f));
	void PostDuplicate() override;

	// 서브클래스 override hook — 베이스의 GameMode 통지 외 추가 동작이 필요할 때.
	virtual void OnPossessedPawnEntered(APawn* Pawn) {}
	virtual void OnPossessedPawnExited(APawn* Pawn) {}

	UBoxComponent* GetTriggerBox() const { return TriggerBox; }

	// 게임모드가 트리거 종류를 구분할 때 사용 (씬에 직렬화).
	FName GetTriggerTag() const { return TriggerTag; }
	void SetTriggerTag(const FName& InTag) { TriggerTag = InTag; }

	void Serialize(FArchive& Ar) override;

protected:
	// 델리게이트 시그니처 — UPrimitiveComponent의 Begin/End Overlap에 매칭.
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
	FName TriggerTag;  // 직렬화 — 디자이너가 씬에서 식별자를 지정
};
