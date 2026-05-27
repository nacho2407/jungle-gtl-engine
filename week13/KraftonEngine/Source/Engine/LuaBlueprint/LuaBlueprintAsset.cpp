#include "LuaBlueprint/LuaBlueprintAsset.h"

#include "LuaBlueprint/LuaBlueprintCompiler.h"
#include "Object/GarbageCollection.h"
#include "Serialization/Archive.h"

#include <algorithm>

FArchive& operator<<(FArchive& Ar, TArray<FLuaBlueprintPin>& Array);
FArchive& operator<<(FArchive& Ar, TArray<FLuaBlueprintLink>& Array);
FArchive& operator<<(FArchive& Ar, TArray<FLuaBlueprintVariable>& Array);
FArchive& operator<<(FArchive& Ar, TArray<FLuaBlueprintNode>& Array);
FArchive& operator<<(FArchive& Ar, TArray<FLuaBlueprintDiagnostic>& Array);

FArchive& operator<<(FArchive& Ar, FLuaBlueprintPin& Pin)
{
	Ar << Pin.PinId;
	Ar << Pin.OwningNodeId;
	Ar << Pin.Kind;
	Ar << Pin.Type;
	Ar << Pin.DisplayName;
	Ar << Pin.DefaultBool;
	Ar << Pin.DefaultInt;
	Ar << Pin.DefaultFloat;
	Ar << Pin.DefaultString;
	Ar << Pin.DefaultVector;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FLuaBlueprintLink& Link)
{
	Ar << Link.LinkId;
	Ar << Link.FromPinId;
	Ar << Link.ToPinId;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FLuaBlueprintVariable& Variable)
{
	Ar << Variable.Name;
	Ar << Variable.Type;
	Ar << Variable.bStrongObject;
	Ar << Variable.BoolValue;
	Ar << Variable.IntValue;
	Ar << Variable.FloatValue;
	Ar << Variable.StringValue;
	Ar << Variable.VectorValue;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FLuaBlueprintNode& Node)
{
	Ar << Node.NodeId;
	Ar << Node.Type;
	Ar << Node.DisplayName;
	Ar << Node.PosX;
	Ar << Node.PosY;
	Ar << Node.Pins;
	Ar << Node.NameValue;
	Ar << Node.StringValue;
	Ar << Node.BoolValue;
	Ar << Node.IntValue;
	Ar << Node.FloatValue;
	Ar << Node.VectorValue;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FLuaBlueprintDiagnostic& Diagnostic)
{
	Ar << Diagnostic.Severity;
	Ar << Diagnostic.NodeId;
	Ar << Diagnostic.Message;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, TArray<FLuaBlueprintPin>& Array)
{
	uint32 N = static_cast<uint32>(Array.size());
	Ar << N;
	if (Ar.IsLoading()) Array.resize(N);
	for (auto& Item : Array) Ar << Item;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, TArray<FLuaBlueprintLink>& Array)
{
	uint32 N = static_cast<uint32>(Array.size());
	Ar << N;
	if (Ar.IsLoading()) Array.resize(N);
	for (auto& Item : Array) Ar << Item;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, TArray<FLuaBlueprintVariable>& Array)
{
	uint32 N = static_cast<uint32>(Array.size());
	Ar << N;
	if (Ar.IsLoading()) Array.resize(N);
	for (auto& Item : Array) Ar << Item;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, TArray<FLuaBlueprintNode>& Array)
{
	uint32 N = static_cast<uint32>(Array.size());
	Ar << N;
	if (Ar.IsLoading()) Array.resize(N);
	for (auto& Item : Array) Ar << Item;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, TArray<FLuaBlueprintDiagnostic>& Array)
{
	uint32 N = static_cast<uint32>(Array.size());
	Ar << N;
	if (Ar.IsLoading()) Array.resize(N);
	for (auto& Item : Array) Ar << Item;
	return Ar;
}

namespace
{
	const char* NodeTypeDisplayName(ELuaBlueprintNodeType Type)
	{
		switch (Type)
		{
		case ELuaBlueprintNodeType::EventBeginPlay:        return "Event BeginPlay";
		case ELuaBlueprintNodeType::EventTick:             return "Event Tick";
		case ELuaBlueprintNodeType::EventEndPlay:          return "Event EndPlay";
		case ELuaBlueprintNodeType::EventOverlap:          return "Event Overlap";
		case ELuaBlueprintNodeType::EventEndOverlap:       return "Event EndOverlap";
		case ELuaBlueprintNodeType::EventHit:              return "Event Hit";
		case ELuaBlueprintNodeType::EventEndHit:           return "Event EndHit";
		case ELuaBlueprintNodeType::Sequence:              return "Sequence";
		case ELuaBlueprintNodeType::Branch:                return "Branch";
		case ELuaBlueprintNodeType::ForLoop:               return "For Loop";
		case ELuaBlueprintNodeType::WhileLoop:             return "While Loop";
		case ELuaBlueprintNodeType::PrintString:           return "Print String";
		case ELuaBlueprintNodeType::LiteralBool:           return "Bool";
		case ELuaBlueprintNodeType::LiteralInt:            return "Int";
		case ELuaBlueprintNodeType::LiteralFloat:          return "Float";
		case ELuaBlueprintNodeType::LiteralString:         return "String";
		case ELuaBlueprintNodeType::LiteralVector:         return "Vector";
		case ELuaBlueprintNodeType::GetVariable:           return "Get Variable";
		case ELuaBlueprintNodeType::SetVariable:           return "Set Variable";
		case ELuaBlueprintNodeType::GetProperty:           return "Get Property";
		case ELuaBlueprintNodeType::SetProperty:           return "Set Property";
		case ELuaBlueprintNodeType::CallFunction:          return "Call Function";
		case ELuaBlueprintNodeType::CallFunctionSignature: return "Call Signature";
		case ELuaBlueprintNodeType::Self:                  return "Self (Owning Actor)";
		case ELuaBlueprintNodeType::AddFloat:              return "Float + Float";
		case ELuaBlueprintNodeType::SubtractFloat:         return "Float - Float";
		case ELuaBlueprintNodeType::MultiplyFloat:         return "Float * Float";
		case ELuaBlueprintNodeType::DivideFloat:           return "Float / Float";
		case ELuaBlueprintNodeType::AddInt:                return "Int + Int";
		case ELuaBlueprintNodeType::SubtractInt:           return "Int - Int";
		case ELuaBlueprintNodeType::MultiplyInt:           return "Int * Int";
		case ELuaBlueprintNodeType::DivideInt:             return "Int / Int";
		case ELuaBlueprintNodeType::ModInt:                return "Int % Int";
		case ELuaBlueprintNodeType::EqualFloat:            return "Float == Float";
		case ELuaBlueprintNodeType::NotEqualFloat:         return "Float != Float";
		case ELuaBlueprintNodeType::LessFloat:             return "Float < Float";
		case ELuaBlueprintNodeType::GreaterFloat:          return "Float > Float";
		case ELuaBlueprintNodeType::LessEqualFloat:        return "Float <= Float";
		case ELuaBlueprintNodeType::GreaterEqualFloat:     return "Float >= Float";
		case ELuaBlueprintNodeType::EqualInt:              return "Int == Int";
		case ELuaBlueprintNodeType::NotEqualInt:           return "Int != Int";
		case ELuaBlueprintNodeType::LessInt:               return "Int < Int";
		case ELuaBlueprintNodeType::GreaterInt:            return "Int > Int";
		case ELuaBlueprintNodeType::And:                   return "Bool AND Bool";
		case ELuaBlueprintNodeType::Or:                    return "Bool OR Bool";
		case ELuaBlueprintNodeType::Not:                   return "NOT Bool";
		case ELuaBlueprintNodeType::AppendString:          return "Append String";
		case ELuaBlueprintNodeType::MakeVector:            return "Make Vector";
		case ELuaBlueprintNodeType::BreakVector:           return "Break Vector";
		case ELuaBlueprintNodeType::AddVector:             return "Vector + Vector";
		case ELuaBlueprintNodeType::SubtractVector:        return "Vector - Vector";
		case ELuaBlueprintNodeType::ScaleVector:           return "Vector * Float";
		case ELuaBlueprintNodeType::DotVector:             return "Dot";
		case ELuaBlueprintNodeType::CrossVector:           return "Cross";
		case ELuaBlueprintNodeType::VectorLength:          return "Vector Length";
		case ELuaBlueprintNodeType::NormalizeVector:       return "Normalize";
		case ELuaBlueprintNodeType::SpawnActor:            return "Spawn Actor";
		case ELuaBlueprintNodeType::DestroyActor:          return "Destroy Actor";
		case ELuaBlueprintNodeType::FindActorByName:       return "Find Actor by Name";
		case ELuaBlueprintNodeType::FindActorByClass:      return "Find Actor of Class";
		case ELuaBlueprintNodeType::FindActorByTag:        return "Find Actor with Tag";
		case ELuaBlueprintNodeType::FindActorsByTag:       return "Find Actors with Tag";
		case ELuaBlueprintNodeType::FindActorsByClass:     return "Find Actors of Class";
		case ELuaBlueprintNodeType::GetActorLocation:      return "Get Actor Location";
		case ELuaBlueprintNodeType::SetActorLocation:      return "Set Actor Location";
		case ELuaBlueprintNodeType::GetActorRotation:      return "Get Actor Rotation";
		case ELuaBlueprintNodeType::SetActorRotation:      return "Set Actor Rotation";
		case ELuaBlueprintNodeType::GetActorScale:         return "Get Actor Scale";
		case ELuaBlueprintNodeType::SetActorScale:         return "Set Actor Scale";
		case ELuaBlueprintNodeType::GetActorForward:       return "Get Actor Forward";
		case ELuaBlueprintNodeType::GetActorRight:         return "Get Actor Right";
		case ELuaBlueprintNodeType::AddActorWorldOffset:   return "Add Actor World Offset";
		case ELuaBlueprintNodeType::ActorHasTag:           return "Actor Has Tag";
		case ELuaBlueprintNodeType::ActorAddTag:           return "Actor Add Tag";
		case ELuaBlueprintNodeType::ActorRemoveTag:        return "Actor Remove Tag";
		case ELuaBlueprintNodeType::GetActorName:          return "Get Actor Name";
		case ELuaBlueprintNodeType::GetOwnerActor:         return "Get Owner Actor";
		case ELuaBlueprintNodeType::IsValid:               return "Is Valid";
		case ELuaBlueprintNodeType::Cast:                  return "Cast";
		case ELuaBlueprintNodeType::GetRootComponent:      return "Get Root Component";
		case ELuaBlueprintNodeType::GetComponentByName:    return "Get Component by Name";
		case ELuaBlueprintNodeType::GetPrimitiveComponent: return "Get Primitive Component";
		case ELuaBlueprintNodeType::ActivateComponent:     return "Activate";
		case ELuaBlueprintNodeType::DeactivateComponent:   return "Deactivate";
		case ELuaBlueprintNodeType::AddForce:              return "Add Force";
		case ELuaBlueprintNodeType::AddTorque:             return "Add Torque";
		case ELuaBlueprintNodeType::GetLinearVelocity:     return "Get Linear Velocity";
		case ELuaBlueprintNodeType::SetLinearVelocity:     return "Set Linear Velocity";
		case ELuaBlueprintNodeType::GetMass:               return "Get Mass";
		case ELuaBlueprintNodeType::SetSimulatePhysics:    return "Set Simulate Physics";
		case ELuaBlueprintNodeType::Lerp:                  return "Lerp";
		case ELuaBlueprintNodeType::Clamp:                 return "Clamp";
		case ELuaBlueprintNodeType::Min:                   return "Min";
		case ELuaBlueprintNodeType::Max:                   return "Max";
		case ELuaBlueprintNodeType::RandomFloat:           return "Random Float";
		case ELuaBlueprintNodeType::RandomInt:             return "Random Int";
		case ELuaBlueprintNodeType::Sin:                   return "Sin";
		case ELuaBlueprintNodeType::Cos:                   return "Cos";
		case ELuaBlueprintNodeType::Sqrt:                  return "Sqrt";
		case ELuaBlueprintNodeType::AbsFloat:              return "Abs";
		case ELuaBlueprintNodeType::Floor:                 return "Floor";
		case ELuaBlueprintNodeType::Ceil:                  return "Ceil";
		case ELuaBlueprintNodeType::Distance:              return "Distance";
		case ELuaBlueprintNodeType::GetGameTime:           return "Get Game Time";
		case ELuaBlueprintNodeType::ForEachActorByClass:   return "For Each Actor (Class)";
		case ELuaBlueprintNodeType::ForEachActorByTag:     return "For Each Actor (Tag)";
		}
		return "Node";
	}
}

FLuaBlueprintNode* ULuaBlueprintAsset::AddNode(ELuaBlueprintNodeType Type, const FName& DisplayName, float X, float Y)
{
	FLuaBlueprintNode Node;
	Node.NodeId      = AllocateId();
	Node.Type        = Type;
	Node.DisplayName = DisplayName;
	Node.PosX        = X;
	Node.PosY        = Y;
	Nodes.push_back(std::move(Node));
	BumpVersion();
	return &Nodes.back();
}

FLuaBlueprintPin* ULuaBlueprintAsset::AddPin(FLuaBlueprintNode& Node, ELuaBlueprintPinKind Kind, ELuaBlueprintPinType PinType, const FName& DisplayName)
{
	FLuaBlueprintPin Pin;
	Pin.PinId        = AllocateId();
	Pin.OwningNodeId = Node.NodeId;
	Pin.Kind         = Kind;
	Pin.Type         = PinType;
	Pin.DisplayName  = DisplayName;
	Node.Pins.push_back(std::move(Pin));
	BumpVersion();
	return &Node.Pins.back();
}

FLuaBlueprintLink* ULuaBlueprintAsset::AddLink(uint32 FromPinId, uint32 ToPinId)
{
	FLuaBlueprintLink Link;
	Link.LinkId    = AllocateId();
	Link.FromPinId = FromPinId;
	Link.ToPinId   = ToPinId;
	Links.push_back(std::move(Link));
	BumpVersion();
	return &Links.back();
}

FLuaBlueprintVariable* ULuaBlueprintAsset::AddVariable(const FName& Name, ELuaBlueprintPinType Type)
{
	FLuaBlueprintVariable Variable;
	Variable.Name = Name;
	Variable.Type = Type;
	Variables.push_back(std::move(Variable));
	BumpVersion();
	return &Variables.back();
}

FLuaBlueprintNode* ULuaBlueprintAsset::AddNodeOfType(ELuaBlueprintNodeType Type, float X, float Y)
{
	FLuaBlueprintNode* N = AddNode(Type, FName(NodeTypeDisplayName(Type)), X, Y);
	if (!N)
	{
		return nullptr;
	}

	switch (Type)
	{
	case ELuaBlueprintNodeType::EventBeginPlay:
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec, FName("Then"));
		break;
	case ELuaBlueprintNodeType::EventTick:
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec, FName("Then"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Float, FName("DeltaTime"));
		break;
	case ELuaBlueprintNodeType::EventEndPlay:
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec, FName("Then"));
		break;
	case ELuaBlueprintNodeType::EventOverlap:
	case ELuaBlueprintNodeType::EventEndOverlap:
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec, FName("Then"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Object, FName("OtherActor"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Object, FName("OverlappedComponent"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Object, FName("OtherComp"));
		break;
	case ELuaBlueprintNodeType::EventHit:
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec, FName("Then"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Object, FName("OtherActor"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Object, FName("HitComponent"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Object, FName("OtherComp"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Vector, FName("NormalImpulse"));
		break;
	case ELuaBlueprintNodeType::EventEndHit:
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec, FName("Then"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Object, FName("OtherActor"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Object, FName("HitComponent"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Object, FName("OtherComp"));
		break;
	case ELuaBlueprintNodeType::Sequence:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Exec, FName("In"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec, FName("Then0"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec, FName("Then1"));
		break;
	case ELuaBlueprintNodeType::Branch:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Exec, FName("In"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Bool, FName("Condition"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec, FName("True"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec, FName("False"));
		break;
	case ELuaBlueprintNodeType::ForLoop:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Exec,  FName("In"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Int,   FName("First"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Int,   FName("Last"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec,  FName("Loop"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Int,   FName("Index"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec,  FName("Completed"));
		break;
	case ELuaBlueprintNodeType::WhileLoop:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Exec, FName("In"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Bool, FName("Condition"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec, FName("Loop"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec, FName("Completed"));
		break;
	case ELuaBlueprintNodeType::PrintString:
		N->StringValue = "Hello Lua Blueprint";
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Exec, FName("In"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::String, FName("Text"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec, FName("Then"));
		break;
	case ELuaBlueprintNodeType::SetVariable:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Exec, FName("In"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Any, FName("Value"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec, FName("Then"));
		break;
	case ELuaBlueprintNodeType::SetProperty:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Exec, FName("In"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Object, FName("Target"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Any, FName("Value"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec, FName("Then"));
		break;
	case ELuaBlueprintNodeType::CallFunction:
	case ELuaBlueprintNodeType::CallFunctionSignature:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Exec, FName("In"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Object, FName("Target"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Any, FName("Arg0"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Any, FName("Arg1"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Any, FName("Arg2"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec, FName("Then"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Any, FName("Result"));
		break;
	case ELuaBlueprintNodeType::GetVariable:
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Any, FName("Value"));
		break;
	case ELuaBlueprintNodeType::LiteralBool:
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Bool, FName("Value"));
		break;
	case ELuaBlueprintNodeType::LiteralInt:
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Int, FName("Value"));
		break;
	case ELuaBlueprintNodeType::LiteralFloat:
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Float, FName("Value"));
		break;
	case ELuaBlueprintNodeType::LiteralString:
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::String, FName("Value"));
		break;
	case ELuaBlueprintNodeType::LiteralVector:
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Vector, FName("Value"));
		break;
	case ELuaBlueprintNodeType::GetProperty:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Object, FName("Target"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Any, FName("Value"));
		break;
	case ELuaBlueprintNodeType::AddFloat:
	case ELuaBlueprintNodeType::SubtractFloat:
	case ELuaBlueprintNodeType::MultiplyFloat:
	case ELuaBlueprintNodeType::DivideFloat:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Float, FName("A"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Float, FName("B"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Float, FName("Value"));
		break;
	case ELuaBlueprintNodeType::AddInt:
	case ELuaBlueprintNodeType::SubtractInt:
	case ELuaBlueprintNodeType::MultiplyInt:
	case ELuaBlueprintNodeType::DivideInt:
	case ELuaBlueprintNodeType::ModInt:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Int, FName("A"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Int, FName("B"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Int, FName("Value"));
		break;
	case ELuaBlueprintNodeType::EqualFloat:
	case ELuaBlueprintNodeType::NotEqualFloat:
	case ELuaBlueprintNodeType::LessFloat:
	case ELuaBlueprintNodeType::GreaterFloat:
	case ELuaBlueprintNodeType::LessEqualFloat:
	case ELuaBlueprintNodeType::GreaterEqualFloat:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Float, FName("A"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Float, FName("B"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Bool,  FName("Result"));
		break;
	case ELuaBlueprintNodeType::EqualInt:
	case ELuaBlueprintNodeType::NotEqualInt:
	case ELuaBlueprintNodeType::LessInt:
	case ELuaBlueprintNodeType::GreaterInt:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Int,  FName("A"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Int,  FName("B"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Bool, FName("Result"));
		break;
	case ELuaBlueprintNodeType::And:
	case ELuaBlueprintNodeType::Or:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Bool, FName("A"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Bool, FName("B"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Bool, FName("Result"));
		break;
	case ELuaBlueprintNodeType::Not:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Bool, FName("A"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Bool, FName("Result"));
		break;
	case ELuaBlueprintNodeType::AppendString:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::String, FName("A"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::String, FName("B"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::String, FName("Value"));
		break;
	case ELuaBlueprintNodeType::MakeVector:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Float,  FName("X"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Float,  FName("Y"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Float,  FName("Z"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Vector, FName("Value"));
		break;
	case ELuaBlueprintNodeType::BreakVector:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Vector, FName("V"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Float,  FName("X"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Float,  FName("Y"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Float,  FName("Z"));
		break;
	case ELuaBlueprintNodeType::AddVector:
	case ELuaBlueprintNodeType::SubtractVector:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Vector, FName("A"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Vector, FName("B"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Vector, FName("Value"));
		break;
	case ELuaBlueprintNodeType::ScaleVector:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Vector, FName("V"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Float,  FName("Scale"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Vector, FName("Value"));
		break;
	case ELuaBlueprintNodeType::DotVector:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Vector, FName("A"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Vector, FName("B"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Float,  FName("Value"));
		break;
	case ELuaBlueprintNodeType::CrossVector:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Vector, FName("A"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Vector, FName("B"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Vector, FName("Value"));
		break;
	case ELuaBlueprintNodeType::VectorLength:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Vector, FName("V"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Float,  FName("Value"));
		break;
	case ELuaBlueprintNodeType::NormalizeVector:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Vector, FName("V"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Vector, FName("Value"));
		break;
	case ELuaBlueprintNodeType::Self:
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Object, FName("Actor"));
		break;

	// ── Actor ──
	case ELuaBlueprintNodeType::SpawnActor:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Exec,   FName("In"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec,   FName("Then"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Object, FName("Actor"));
		break;
	case ELuaBlueprintNodeType::DestroyActor:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Exec,   FName("In"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Object, FName("Target"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec,   FName("Then"));
		break;
	case ELuaBlueprintNodeType::FindActorByName:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::String, FName("Name"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Object, FName("Actor"));
		break;
	case ELuaBlueprintNodeType::FindActorByClass:
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Object, FName("Actor"));
		break;
	case ELuaBlueprintNodeType::FindActorByTag:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::String, FName("Tag"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Object, FName("Actor"));
		break;
	case ELuaBlueprintNodeType::FindActorsByTag:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::String, FName("Tag"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Object, FName("Actors"));
		break;
	case ELuaBlueprintNodeType::FindActorsByClass:
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Object, FName("Actors"));
		break;
	case ELuaBlueprintNodeType::GetActorLocation:
	case ELuaBlueprintNodeType::GetActorRotation:
	case ELuaBlueprintNodeType::GetActorScale:
	case ELuaBlueprintNodeType::GetActorForward:
	case ELuaBlueprintNodeType::GetActorRight:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Object, FName("Target"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Vector, FName("Value"));
		break;
	case ELuaBlueprintNodeType::SetActorLocation:
	case ELuaBlueprintNodeType::SetActorRotation:
	case ELuaBlueprintNodeType::SetActorScale:
	case ELuaBlueprintNodeType::AddActorWorldOffset:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Exec,   FName("In"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Object, FName("Target"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Vector, FName("Value"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec,   FName("Then"));
		break;
	case ELuaBlueprintNodeType::ActorHasTag:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Object, FName("Target"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::String, FName("Tag"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Bool,   FName("Result"));
		break;
	case ELuaBlueprintNodeType::ActorAddTag:
	case ELuaBlueprintNodeType::ActorRemoveTag:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Exec,   FName("In"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Object, FName("Target"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::String, FName("Tag"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec,   FName("Then"));
		break;
	case ELuaBlueprintNodeType::GetActorName:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Object, FName("Target"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::String, FName("Name"));
		break;
	case ELuaBlueprintNodeType::GetOwnerActor:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Object, FName("Component"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Object, FName("Owner"));
		break;

	// ── Object utility ──
	case ELuaBlueprintNodeType::IsValid:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Object, FName("Target"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Bool,   FName("Valid"));
		break;
	case ELuaBlueprintNodeType::Cast:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Exec,   FName("In"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Object, FName("Target"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec,   FName("Success"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec,   FName("Failed"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Object, FName("Result"));
		break;

	// ── Component ──
	case ELuaBlueprintNodeType::GetRootComponent:
	case ELuaBlueprintNodeType::GetPrimitiveComponent:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Object, FName("Actor"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Object, FName("Component"));
		break;
	case ELuaBlueprintNodeType::GetComponentByName:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Object, FName("Actor"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::String, FName("Name"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Object, FName("Component"));
		break;
	case ELuaBlueprintNodeType::ActivateComponent:
	case ELuaBlueprintNodeType::DeactivateComponent:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Exec,   FName("In"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Object, FName("Component"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec,   FName("Then"));
		break;
	case ELuaBlueprintNodeType::AddForce:
	case ELuaBlueprintNodeType::AddTorque:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Exec,   FName("In"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Object, FName("Component"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Vector, FName("Vector"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec,   FName("Then"));
		break;
	case ELuaBlueprintNodeType::GetLinearVelocity:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Object, FName("Component"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Vector, FName("Velocity"));
		break;
	case ELuaBlueprintNodeType::SetLinearVelocity:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Exec,   FName("In"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Object, FName("Component"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Vector, FName("Velocity"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec,   FName("Then"));
		break;
	case ELuaBlueprintNodeType::GetMass:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Object, FName("Component"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Float,  FName("Mass"));
		break;
	case ELuaBlueprintNodeType::SetSimulatePhysics:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Exec,   FName("In"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Object, FName("Component"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Bool,   FName("Simulate"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec,   FName("Then"));
		break;

	// ── Math util ──
	case ELuaBlueprintNodeType::Lerp:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Float, FName("A"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Float, FName("B"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Float, FName("Alpha"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Float, FName("Value"));
		break;
	case ELuaBlueprintNodeType::Clamp:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Float, FName("Value"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Float, FName("Min"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Float, FName("Max"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Float, FName("Value"));
		break;
	case ELuaBlueprintNodeType::Min:
	case ELuaBlueprintNodeType::Max:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Float, FName("A"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Float, FName("B"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Float, FName("Value"));
		break;
	case ELuaBlueprintNodeType::RandomFloat:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Float, FName("Min"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Float, FName("Max"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Float, FName("Value"));
		break;
	case ELuaBlueprintNodeType::RandomInt:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Int, FName("Min"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Int, FName("Max"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Int, FName("Value"));
		break;
	case ELuaBlueprintNodeType::Sin:
	case ELuaBlueprintNodeType::Cos:
	case ELuaBlueprintNodeType::Sqrt:
	case ELuaBlueprintNodeType::AbsFloat:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Float, FName("Value"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Float, FName("Value"));
		break;
	case ELuaBlueprintNodeType::Floor:
	case ELuaBlueprintNodeType::Ceil:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Float, FName("Value"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Int,   FName("Value"));
		break;
	case ELuaBlueprintNodeType::Distance:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Vector, FName("A"));
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Vector, FName("B"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Float,  FName("Distance"));
		break;
	case ELuaBlueprintNodeType::GetGameTime:
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Float, FName("Seconds"));
		break;

	// ── ForEach ──
	case ELuaBlueprintNodeType::ForEachActorByClass:
	case ELuaBlueprintNodeType::ForEachActorByTag:
		AddPin(*N, ELuaBlueprintPinKind::Input,  ELuaBlueprintPinType::Exec,   FName("In"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec,   FName("Loop"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Object, FName("Actor"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Int,    FName("Index"));
		AddPin(*N, ELuaBlueprintPinKind::Output, ELuaBlueprintPinType::Exec,   FName("Completed"));
		break;
	}
	return N;
}

void ULuaBlueprintAsset::InitializeDefault()
{
	Nodes.clear();
	Links.clear();
	Variables.clear();
	Diagnostics.clear();
	GeneratedLuaSource.clear();
	NextId = 1;

	uint32 BeginThenPin = 0;
	if (FLuaBlueprintNode* Begin = AddNodeOfType(ELuaBlueprintNodeType::EventBeginPlay, -280.0f, 0.0f))
	{
		if (!Begin->Pins.empty()) BeginThenPin = Begin->Pins.front().PinId;
	}

	uint32 PrintInPin = 0;
	if (FLuaBlueprintNode* Print = AddNodeOfType(ELuaBlueprintNodeType::PrintString, 0.0f, 0.0f))
	{
		Print->StringValue = "Lua Blueprint BeginPlay";
		for (FLuaBlueprintPin& Pin : Print->Pins)
		{
			if (Pin.Kind == ELuaBlueprintPinKind::Input && Pin.Type == ELuaBlueprintPinType::Exec)
			{
				PrintInPin = Pin.PinId;
			}
			else if (Pin.Kind == ELuaBlueprintPinKind::Input && Pin.Type == ELuaBlueprintPinType::String && Pin.DisplayName.ToString() == "Text")
			{
				Pin.DefaultString = Print->StringValue;
			}
		}
	}

	if (BeginThenPin && PrintInPin)
	{
		AddLink(BeginThenPin, PrintInPin);
	}

	Compile();
}

bool ULuaBlueprintAsset::RemoveNode(uint32 NodeId)
{
	if (NodeId == 0) return false;

	TArray<uint32> PinIds;
	for (const FLuaBlueprintNode& Node : Nodes)
	{
		if (Node.NodeId != NodeId) continue;
		for (const FLuaBlueprintPin& Pin : Node.Pins) PinIds.push_back(Pin.PinId);
		break;
	}

	const size_t BeforeNodes = Nodes.size();
	Links.erase(std::remove_if(Links.begin(), Links.end(), [&PinIds](const FLuaBlueprintLink& Link)
	{
		for (uint32 PinId : PinIds)
		{
			if (Link.FromPinId == PinId || Link.ToPinId == PinId) return true;
		}
		return false;
	}), Links.end());

	Nodes.erase(std::remove_if(Nodes.begin(), Nodes.end(), [NodeId](const FLuaBlueprintNode& Node)
	{
		return Node.NodeId == NodeId;
	}), Nodes.end());

	const bool bRemoved = Nodes.size() != BeforeNodes;
	if (bRemoved) BumpVersion();
	return bRemoved;
}

bool ULuaBlueprintAsset::RemoveLink(uint32 LinkId)
{
	if (LinkId == 0) return false;
	const size_t Before = Links.size();
	Links.erase(std::remove_if(Links.begin(), Links.end(), [LinkId](const FLuaBlueprintLink& Link)
	{
		return Link.LinkId == LinkId;
	}), Links.end());
	const bool bRemoved = Links.size() != Before;
	if (bRemoved) BumpVersion();
	return bRemoved;
}

bool ULuaBlueprintAsset::CanLinkPins(uint32 PinAId, uint32 PinBId, uint32* OutFromPinId, uint32* OutToPinId) const
{
	if (PinAId == 0 || PinBId == 0 || PinAId == PinBId) return false;

	const FLuaBlueprintPin* A = FindPin(PinAId);
	const FLuaBlueprintPin* B = FindPin(PinBId);
	if (!A || !B) return false;
	if (A->OwningNodeId == B->OwningNodeId) return false;
	if (A->Kind == B->Kind) return false;

	const FLuaBlueprintPin* From = (A->Kind == ELuaBlueprintPinKind::Output) ? A : B;
	const FLuaBlueprintPin* To   = (From == A) ? B : A;

	if (From->Type != ELuaBlueprintPinType::Any && To->Type != ELuaBlueprintPinType::Any && From->Type != To->Type)
	{
		return false;
	}

	// Input fan-in 은 1개만 허용. Exec output 은 fan-out 도 1개만 허용 (분기는 Sequence 노드로).
	// 데이터 output 은 여러 input 에 fan-out 가능.
	for (const FLuaBlueprintLink& Link : Links)
	{
		if (Link.FromPinId == From->PinId && Link.ToPinId == To->PinId) return false;
		if (Link.ToPinId == To->PinId) return false;
		if (From->Type == ELuaBlueprintPinType::Exec && Link.FromPinId == From->PinId) return false;
	}

	if (OutFromPinId) *OutFromPinId = From->PinId;
	if (OutToPinId)   *OutToPinId   = To->PinId;
	return true;
}

FLuaBlueprintNode* ULuaBlueprintAsset::FindNode(uint32 NodeId)
{
	if (NodeId == 0) return nullptr;
	for (FLuaBlueprintNode& Node : Nodes)
	{
		if (Node.NodeId == NodeId) return &Node;
	}
	return nullptr;
}

const FLuaBlueprintNode* ULuaBlueprintAsset::FindNode(uint32 NodeId) const
{
	if (NodeId == 0) return nullptr;
	for (const FLuaBlueprintNode& Node : Nodes)
	{
		if (Node.NodeId == NodeId) return &Node;
	}
	return nullptr;
}

FLuaBlueprintPin* ULuaBlueprintAsset::FindPin(uint32 PinId)
{
	if (PinId == 0) return nullptr;
	for (FLuaBlueprintNode& Node : Nodes)
	{
		for (FLuaBlueprintPin& Pin : Node.Pins)
		{
			if (Pin.PinId == PinId) return &Pin;
		}
	}
	return nullptr;
}

const FLuaBlueprintPin* ULuaBlueprintAsset::FindPin(uint32 PinId) const
{
	if (PinId == 0) return nullptr;
	for (const FLuaBlueprintNode& Node : Nodes)
	{
		for (const FLuaBlueprintPin& Pin : Node.Pins)
		{
			if (Pin.PinId == PinId) return &Pin;
		}
	}
	return nullptr;
}

const FLuaBlueprintLink* ULuaBlueprintAsset::FindLinkToInput(uint32 InputPinId) const
{
	if (InputPinId == 0) return nullptr;
	for (const FLuaBlueprintLink& Link : Links)
	{
		if (Link.ToPinId == InputPinId) return &Link;
	}
	return nullptr;
}

const FLuaBlueprintLink* ULuaBlueprintAsset::FindFirstLinkFromOutput(uint32 OutputPinId) const
{
	if (OutputPinId == 0) return nullptr;
	for (const FLuaBlueprintLink& Link : Links)
	{
		if (Link.FromPinId == OutputPinId) return &Link;
	}
	return nullptr;
}

bool ULuaBlueprintAsset::Compile()
{
	FLuaBlueprintCompileResult Result = FLuaBlueprintCompiler::Compile(*this);
	SetCompileResult(Result.GeneratedLuaSource, std::move(Result.Diagnostics), Result.bSuccess);
	return Result.bSuccess;
}

bool ULuaBlueprintAsset::HasCompileErrors() const
{
	for (const FLuaBlueprintDiagnostic& Diagnostic : Diagnostics)
	{
		if (Diagnostic.Severity == ELuaBlueprintDiagnosticSeverity::Error)
		{
			return true;
		}
	}
	return false;
}

void ULuaBlueprintAsset::SetCompileResult(const FString& InSource, TArray<FLuaBlueprintDiagnostic>&& InDiagnostics, bool bSuccess)
{
	GeneratedLuaSource = InSource;
	Diagnostics = std::move(InDiagnostics);
	bCompileDirty = !bSuccess;
}

void ULuaBlueprintAsset::Serialize(FArchive& Ar)
{
	Ar << NextId;
	Ar << Nodes;
	Ar << Links;
	Ar << Variables;
	Ar << GeneratedLuaSource;
	if (Ar.IsLoading())
	{
		bCompileDirty = GeneratedLuaSource.empty();
		Diagnostics.clear();
	}
}

void ULuaBlueprintAsset::AddReferencedObjects(FReferenceCollector& Collector)
{
	UObject::AddReferencedObjects(Collector);
}
