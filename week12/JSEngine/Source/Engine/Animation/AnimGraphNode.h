#pragma once
#include "Core/CoreMinimal.h"

enum class EAnimGraphNodeType
{
	OutputPose,
	SequencePlayer,
	StateMachine,
};

enum class EAnimTransitionConditionType
{
    Always,
	BoolParameter,
	FloatGreater,
	FloatLess,
	LuaFunction,
};

struct FAnimTransitionConditionDesc
{
	EAnimTransitionConditionType Type = EAnimTransitionConditionType::Always;
	FString ParameterName;
	bool BoolValue = true;
	float Threshold = 0.0f;
	FString LuaFunctionName;
};

struct FAnimStateTransitionDesc
{
	int32 FromStateId = -1;
	int32 ToStateId = -1;
	float BlendTime = 0.2f;
	int32 Priority = 0;
	FAnimTransitionConditionDesc Condition;
};

struct FAnimStateDesc
{
	int32 StateId = -1;
	FString Name;
	FString AnimationPath;
	FVector2 Position = FVector2(0.0f, 0.0f);
};

struct FAnimStateMachineDesc
{
	int32 EntryStateId = -1;
	TArray<FAnimStateDesc> States;
	TArray<FAnimStateTransitionDesc> Transitions;
};

struct FAnimGraphNodeDesc
{
	int32 NodeId = -1;
	EAnimGraphNodeType Type = EAnimGraphNodeType::SequencePlayer;
	FString Name;
	FVector2 Position = FVector2(0.0f, 0.0f);

	FString AnimationPath;
	float PlayRate = 1.0f;
	bool bLoop = true;

	int32 InputPoseNodeId = -1;
	
	FAnimStateMachineDesc StateMachine;
};

FString AnimGraphNodeTypeToString(EAnimGraphNodeType Type);
EAnimGraphNodeType AnimGraphNodeTypeFromString(const FString& Value);
FString TransitionConditionTypeToString(EAnimTransitionConditionType Type);
EAnimTransitionConditionType TransitionConditionTypeFromString(const FString& Value);

struct FArchive;
FArchive& operator<<(FArchive& Ar, FAnimTransitionConditionDesc& Desc);
FArchive& operator<<(FArchive& Ar, FAnimStateTransitionDesc& Desc);
FArchive& operator<<(FArchive& Ar, FAnimStateDesc& Desc);
FArchive& operator<<(FArchive& Ar, FAnimStateMachineDesc& Desc);
FArchive& operator<<(FArchive& Ar, FAnimGraphNodeDesc& Desc);