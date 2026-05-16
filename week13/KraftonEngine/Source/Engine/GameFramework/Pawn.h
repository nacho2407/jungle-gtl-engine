#pragma once

#include "GameFramework/AActor.h"
#include "Math/Rotator.h"

class APlayerController;
class UInputComponent;

// ============================================================
// APawn — PlayerController가 Possess할 수 있는 액터의 베이스
//
// 베이스는 빈 껍데기. 차량/캐릭터 등 구체 Pawn은 이 클래스를 상속받아
// 컴포넌트와 제어 로직을 갖춘다 (예: APawnStaticMesh).
//
// "Possessed Pawn" 식별은 TriggerVolume 등에서 IsPossessed()로 한다.
//
// Input 통합:
//   - BeginPlay 가 자동으로 UInputComponent 부착 + SetupInputComponent() 호출.
//   - 자식이 SetupInputComponent override 안에서 BindAxis/BindAction 호출.
//   - Lua: obj:AsPawn():GetInputComponent():BindAction(...) 패턴.
// ============================================================
class APawn : public AActor
{
public:
	DECLARE_CLASS(APawn, AActor)

	APawn() = default;
	~APawn() override = default;

	// PlayerController::Possess가 호출 — 서브클래스가 override해서
	// 입력 활성화/카메라 전환 등을 처리할 수 있다.
	virtual void PossessedBy(APlayerController* PC);
	virtual void UnPossessed();

	// Input 활성화 — BeginPlay 가 UInputComponent 부착 후 호출. 자식이 override.
	// 기본은 no-op — 자식이 mapping/binding 설정.
	virtual void SetupInputComponent() {}

	void BeginPlay() override;

	APlayerController* GetController() const { return Controller; }
	bool IsPossessed() const { return Controller != nullptr; }

	void SetAutoPossessPlayer(bool bIn) { bAutoPossessPlayer = bIn; }
	bool GetAutoPossessPlayer() const { return bAutoPossessPlayer; }

	UInputComponent* GetInputComponent() const { return InputComponent; }

	// Control rotation — UE 패턴. capsule rotation 과 분리된 "사용자가 보고 있는 방향".
	// SpringArm/Camera 가 bUsePawnControlRotation 통해 이걸 사용 → mouse look 이 카메라만 회전.
	// capsule yaw 가 이걸 따라가게 하려면 자식이 bUseControllerRotationYaw 등 옵션으로 toggle.
	virtual FRotator GetControlRotation() const { return ControlRotation; }
	void             SetControlRotation(const FRotator& NewRot) { ControlRotation = NewRot; }

	// 누적 헬퍼 — ACharacter::Tick 등에서 mouse delta * sensitivity 호출.
	void             AddYawInput  (float Value) { ControlRotation.Yaw   += Value; }
	void             AddPitchInput(float Value) { ControlRotation.Pitch += Value; }

	// RootComponent (Character 의 경우 Capsule) 의 rotation 을 ControlRotation 의 해당 axis 로
	// 즉시 set. ACharacter::Tick 의 mouse handling 직후 호출 — 1 frame 지연 없이 반영.
	void             ApplyControllerRotationToRoot();

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void Serialize(FArchive& Ar) override;

	// UE 패턴 — RootComponent rotation 을 매 frame ControlRotation 의 해당 axis 로 즉시 동기화.
	// true 면 마우스 따라 mesh 가 즉시 회전 (ThirdPerson 슈터 패턴).
	// CharacterMovement 의 bOrientRotationToMovement 와 동시 true 면 Movement 가 마지막 우선 —
	// 보통 둘 중 하나만 켜는 것이 일반적.
	bool bUseControllerRotationPitch = true;
	bool bUseControllerRotationYaw   = true;
	bool bUseControllerRotationRoll  = false;

protected:
	APlayerController* Controller = nullptr;  // 직렬화 제외 — 런타임에 PC가 세팅
	bool bAutoPossessPlayer = true;            // 직렬화 — GameMode가 시작 시 자동 Possess할 후보로 사용

	// BeginPlay 가 자동 추가 — 자식의 SetupInputComponent 가 mapping/binding 등록.
	UInputComponent* InputComponent = nullptr;

	// "사용자가 보는 방향" — capsule yaw 와 분리. SpringArm 이 inherit.
	FRotator ControlRotation;
};
