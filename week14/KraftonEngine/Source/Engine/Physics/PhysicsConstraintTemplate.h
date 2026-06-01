#pragma once
#include "Core/Types/CoreTypes.h"
#include "Math/Transform.h"
#include "Object/Object.h"

UENUM()
enum class EAngularConstraintMode : uint8
{
	Free, // 모든 방향 자유 회전
	Limited, // 설정 각도 내에서만
	Locked // 회전 고정
};

#include "Source/Engine/Physics/PhysicsConstraintTemplate.generated.h"

UCLASS()
class UPhysicsConstraintTemplate : public UObject
{
public:
	GENERATED_BODY()
	
	UPhysicsConstraintTemplate() = default;
	~UPhysicsConstraintTemplate() override = default;

	void Serialize(FArchive& Ar) override;
	
	void Setup(FName InParentBone, FName InChildBone, const FTransform& InFrameA, const FTransform& InFrameB, EAngularConstraintMode InMode)
	{
		ParentBoneName = InParentBone;
		ChildBoneName = InChildBone;
		LocalFrameA = InFrameA;
		LocalFrameB = InFrameB;
		AngularMode = InMode;
	}
	
	FName GetParentBoneName() const {return ParentBoneName; }
	FName GetChildBoneName()  const { return ChildBoneName; }
	const FTransform& GetLocalFrameA() const { return LocalFrameA; }
	const FTransform& GetLocalFrameB() const { return LocalFrameB; }
	EAngularConstraintMode GetAngularMode() const { return AngularMode; }
	float GetSwing1Limit() const { return Swing1Limit; }
	float GetSwing2Limit() const { return Swing2Limit; }
	float GetTwistLimit() const { return TwistLimit; }

	void SetParentBoneName(FName InBoneName) { ParentBoneName = InBoneName; }
	void SetChildBoneName(FName InBoneName) { ChildBoneName = InBoneName; }
	void SetLocalFrameA(const FTransform& InFrame) { LocalFrameA = InFrame; }
	void SetLocalFrameB(const FTransform& InFrame) { LocalFrameB = InFrame; }
	void SetAngularMode(EAngularConstraintMode InMode) { AngularMode = InMode; }
	void SetAngularLimits(float InSwing1Limit, float InSwing2Limit, float InTwistLimit);
	
private:
	UPROPERTY(Edit, Save, Category="Constraint", DisplayName="Parent Bone")
	FName ParentBoneName;

	UPROPERTY(Edit, Save, Category="Constraint", DisplayName="Child Bone")
	FName ChildBoneName;

	FTransform LocalFrameA;

	FTransform LocalFrameB;

	UPROPERTY(Edit, Save, Category="Constraint", DisplayName="Angular Mode", Enum=EAngularConstraintMode)
	EAngularConstraintMode AngularMode = EAngularConstraintMode::Limited;

	UPROPERTY(Edit, Save, Category="Constraint", DisplayName="Swing 1 Limit", Min=0.0f, Max=180.0f, Speed=1.0f)
	float Swing1Limit = 45.0f;

	UPROPERTY(Edit, Save, Category="Constraint", DisplayName="Swing 2 Limit", Min=0.0f, Max=180.0f, Speed=1.0f)
	float Swing2Limit = 45.0f;

	UPROPERTY(Edit, Save, Category="Constraint", DisplayName="Twist Limit", Min=0.0f, Max=180.0f, Speed=1.0f)
	float TwistLimit  = 30.0f;
};
