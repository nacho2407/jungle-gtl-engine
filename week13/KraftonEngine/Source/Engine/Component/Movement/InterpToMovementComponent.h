#pragma once
#include "MovementComponent.h"
#include "Math/Vector.h"

#include "Source/Engine/Component/Movement/InterpToMovementComponent.generated.h"
enum class EInterpBehaviour {
	OneShot,
	OneShotReverse,
	Loop,
	PingPong,
};

inline const char* GInterpBehaviourNames[] = { "One Shot", "One Shot Reverse", "Loop", "Ping-Pong" };

UCLASS()
class UInterpToMovementComponent : public UMovementComponent {
public:
	GENERATED_BODY()
	UInterpToMovementComponent() = default;

	// Overrides
	void				BeginPlay() override;
	void				TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	// Control Point Management
	void				AddControlPoint(FVector InControlPoint);
	void				RemoveControlPoint(uint32 Index);
	TArray<FVector>& GetControlPoints() { return ControlPoints; }
	FVector& GetControlPoint(uint32 Index);
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
	UPROPERTY(Edit, Save, Category="Movement", DisplayName="Interp Mode", Type=Enum, EnumNames=GInterpBehaviourNames, EnumCount=4, EnumType=EInterpBehaviour)
	EInterpBehaviour	InterpBehaviour = EInterpBehaviour::OneShot;
	UPROPERTY(Edit, Save, Category="Movement", DisplayName="Control Points", Type=Vec3Array)
	TArray<FVector>		ControlPoints;
	uint32				CurrentPointID = 0;
	uint32				NextPointID = 0;
	UPROPERTY(Edit, Save, Category="Movement", DisplayName="Interp Duration", Min=0.1f, Max=2048.0f, Speed=0.1f)
	float				Duration = 5.0f;		// Does not store an "array" of duration
	float				RotateDuration = 0.f;
	float				Elapsed = 0.f;
	float				TotalDistance = 0;
	float				NextDistRatio = 0;
	bool				bisLerping = false;
	UPROPERTY(Edit, Save, Category="Movement", DisplayName="Auto Activate")
	bool				bAutoActivate = true;
	bool				bPing = true;
	UPROPERTY(Edit, Save, Category="Movement", DisplayName="Orient To Movement")
	bool				bFaceTargetDir = true;

	float				TargetPitch = 0.f;
	float				TargetYaw = 0.f;
};
