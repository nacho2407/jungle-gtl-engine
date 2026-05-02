#pragma once

#include "GameFramework/AActor.h"

class APlayerController;

// ============================================================
// APawn — PlayerController가 Possess할 수 있는 액터의 베이스
//
// 베이스는 빈 껍데기. 차량/캐릭터 등 구체 Pawn은 이 클래스를 상속받아
// 컴포넌트와 제어 로직을 갖춘다 (예: APawnStaticMesh).
//
// "Possessed Pawn" 식별은 TriggerVolume 등에서 IsPossessed()로 한다.
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

	APlayerController* GetController() const { return Controller; }
	bool IsPossessed() const { return Controller != nullptr; }

	void SetAutoPossessPlayer(bool bIn) { bAutoPossessPlayer = bIn; }
	bool GetAutoPossessPlayer() const { return bAutoPossessPlayer; }

	void Serialize(FArchive& Ar) override;

protected:
	APlayerController* Controller = nullptr;  // 직렬화 제외 — 런타임에 PC가 세팅
	bool bAutoPossessPlayer = true;            // 직렬화 — GameMode가 시작 시 자동 Possess할 후보로 사용
};
