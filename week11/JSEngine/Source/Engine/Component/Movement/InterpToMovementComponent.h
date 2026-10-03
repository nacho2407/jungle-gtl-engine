#pragma once
#include "MovementComponent.h"
#include "ReflectionSystem/ReflectionTypeInfo.h"

#include "InterpToMovementComponent.generated.h"

UENUM()
enum class EInterpBehaviour {
	OneShot,
	OneShotReverse,
	Loop,
	PingPong,
};

UCLASS()
class UInterpToMovementComponent : public UMovementComponent {
    GENERATED_BODY_UInterpToMovementComponent()
public:
	DECLARE_CLASS(UInterpToMovementComponent, UMovementComponent)

	UInterpToMovementComponent() = default;

	// Overrides
	void				BeginPlay() override;
	void				TickComponent(float DeltaTime) override;
    void				GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void				PostDuplicate(UObject* Original) override;
	float				GetMaxSpeed() const override { return 0; };

	// Control Point Management
	void				AddControlPoint(FVector InControlPoint);
	void				RemoveControlPoint(uint32 Index);
	TArray<FVector>&	GetControlPoints() { return ControlPoints; }
	FVector&			GetControlPoint(uint32 Index);
	void				SetControlPoint(uint32 Index, FVector InPoint);

	// Interpolation Duration
	float				GetInterpDuration() const { return Duration; }
	void				SetInterpDuration(float InDuration);

	// Interpolation behaviour
	EInterpBehaviour	GetInterpolationBehaviour() const { return InterpBehaviour; }
	void				SetInterpolationBehaviour(EInterpBehaviour InBehaviour);
	bool				IsFacingTargetDir() const { return bFaceTargetDir; }
	void				ShouldFaceTargetDir(bool InBool) { bFaceTargetDir = InBool; }

	// Misc
	void				Initiate();
	bool				IsAutoActivating() const { return bAutoActivate; }
	void				ShouldAutoActivate(bool bActivate) { bAutoActivate = bActivate; }
	void				Reset();
	void				ResetAndHalt();			

private:
	// Used to lerp back and forth when Behaviour is set to PingPong
	void				Ping();
	void				Pong();

	// Determines what to do after reeaching the target destination inside the chain
	void				DestinationReached();

	// Determines what to do after a chain of interpolation has been ended
	void				EndOfChain();

	// Returns the ratio of the distance towards the next control point over the total distance
	void				SetNextDistRatio();

	// Determines the rotation speed proportional to distance ratio
	void				SetRotationSpeed();

	// Tick - Lerp updates
	void				UpdateLerp(float DeltaTime);

	// Rotate (interpolated) towards target direction if flagged.
	// Call before updating PointID
	void				FaceTargetDir(float DeltaTime);

private:
	UPROPERTY(EditAnywhere, Category="Interpolation", DisplayName="Mode")
	EInterpBehaviour	InterpBehaviour		= EInterpBehaviour::OneShot;
	UPROPERTY(EditAnywhere, Category="Interpolation", DisplayName="Control Points")
	TArray<FVector>		ControlPoints;
	uint32				CurrentPointID		= 0;
	uint32				NextPointID			= 0;
	UPROPERTY(EditAnywhere, Category="Interpolation", DisplayName="Duration")
	float				Duration			= 5.0f;		// Does not store an "array" of duration
	float				RotateDuration		= 0.f;
	float				Elapsed				= 0.f;
	float				TotalDistance		= 0;
	float				NextDistRatio		= 0;
	bool				bisLerping			= true;
	UPROPERTY(EditAnywhere, Category="Activation", DisplayName="Auto Activate")
	bool				bAutoActivate		= true;
	bool				bPing				= true;
	UPROPERTY(EditAnywhere, Category="Rotation", DisplayName="Face Target Direction")
	bool				bFaceTargetDir		= true;

	float				TargetPitch			= 0.f;
	float				TargetYaw			= 0.f;
};
