#include "AnimGraphNode.h"

FString AnimGraphNodeTypeToString(EAnimGraphNodeType Type)
{
	switch (Type)
	{
	case EAnimGraphNodeType::OutputPose:
		return "OutputPose";
	case EAnimGraphNodeType::SequencePlayer:
		return "SequencePlayer";
	case EAnimGraphNodeType::StateMachine:
		return "StateMachine";
	default:
		return "Unknown";
	}
}
