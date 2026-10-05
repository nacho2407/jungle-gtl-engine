#pragma once

#include "Core/CoreMinimal.h"
#include "Core/Reflection/ReflectionMacros.h"

UENUM()
enum class EAnimGraphNodeType
{
	OutputPose UMETA(DisplayName = "Output Pose"),
	SequencePlayer UMETA(DisplayName = "Sequence Player"),
	StateMachine UMETA(DisplayName = "State Machine"),
};

UENUM()
enum class EAnimTransitionConditionType
{
	AlwaysTrue UMETA(DisplayName = "Always True"),
	BoolParameter UMETA(DisplayName = "Bool Parameter"),
	FloatGreater UMETA(DisplayName = "Float Greater"),
	FloatLess UMETA(DisplayName = "Float Less"),
	LuaFunction UMETA(DisplayName = "Lua Function"),
	IntEquals UMETA(DisplayName = "Int Equals"),
	IntGreater UMETA(DisplayName = "Int Greater"),
	IntLess UMETA(DisplayName = "Int Less"),
};

USTRUCT()
struct FAnimTransitionConditionDesc
{
	GENERATED_STRUCT_BODY(FAnimTransitionConditionDesc)

	UPROPERTY()
	EAnimTransitionConditionType Type = EAnimTransitionConditionType::AlwaysTrue;
	UPROPERTY()
	FString ParameterName;
	UPROPERTY()
	bool BoolValue = true;
	UPROPERTY()
	float Threshold = 0.0f;
	UPROPERTY()
	int32 IntValue = 0;
	UPROPERTY()
	FString LuaFunctionName;
};

USTRUCT()
struct FAnimStateTransitionDesc
{
	GENERATED_STRUCT_BODY(FAnimStateTransitionDesc)

	UPROPERTY()
	int32 FromStateId = -1;
	UPROPERTY()
	int32 ToStateId = -1;
	UPROPERTY()
	float BlendTime = 0.2f;
	UPROPERTY()
	int32 Priority = 0;
	UPROPERTY()
	FAnimTransitionConditionDesc Condition;
};

USTRUCT()
struct FAnimStateDesc
{
	GENERATED_STRUCT_BODY(FAnimStateDesc)

	UPROPERTY()
	int32 StateId = -1;
	UPROPERTY()
	FString Name;
	UPROPERTY()
	FString AnimationPath;
	UPROPERTY()
	FVector2 Position = FVector2(0.0f, 0.0f);
	UPROPERTY()
	float PlayRate = 1.0f;
	UPROPERTY()
	bool bLoop = true;
	UPROPERTY()
	bool bAutoAdvanceOnEnd = true;
};

USTRUCT()
struct FAnimStateMachineDesc
{
	GENERATED_STRUCT_BODY(FAnimStateMachineDesc)

	UPROPERTY()
	int32 EntryStateId = -1;
	UPROPERTY()
	FVector2 EntryPosition = FVector2(36.0f, 84.0f);
	UPROPERTY()
	TArray<FAnimStateDesc> States;
	UPROPERTY()
	TArray<FAnimStateTransitionDesc> Transitions;
};

USTRUCT()
struct FAnimGraphNodeDesc
{
	GENERATED_STRUCT_BODY(FAnimGraphNodeDesc)

	UPROPERTY(NoEdit)
	int32 NodeId = -1;
	UPROPERTY(NoEdit)
	EAnimGraphNodeType Type = EAnimGraphNodeType::SequencePlayer;
	UPROPERTY(NoEdit)
	FString Name;
	UPROPERTY(NoEdit)
	FVector2 Position = FVector2(0.0f, 0.0f);

	UPROPERTY(NoEdit)
	FString AnimationPath;
	UPROPERTY(NoEdit)
	float PlayRate = 1.0f;
	UPROPERTY(NoEdit)
	bool bLoop = true;

	UPROPERTY(NoEdit)
	int32 InputPoseNodeId = -1;
	UPROPERTY(NoEdit)
	FAnimStateMachineDesc StateMachine;
};

FString AnimGraphNodeTypeToString(EAnimGraphNodeType Type);
