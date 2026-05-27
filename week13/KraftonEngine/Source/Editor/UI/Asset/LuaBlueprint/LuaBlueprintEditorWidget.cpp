#include "Editor/UI/Asset/LuaBlueprint/LuaBlueprintEditorWidget.h"

#include "LuaBlueprint/LuaBlueprintAsset.h"
#include "LuaBlueprint/LuaBlueprintManager.h"
#include "Object/Object.h"

#include "imgui.h"
#include "imgui_internal.h" // BeginDragDropTargetCustom / ImRect
#include "imgui_node_editor.h"

#include "Object/Reflection/UClass.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ed = ax::NodeEditor;

namespace
{
	inline ed::NodeId ToNodeId(uint32 Id) { return static_cast<ed::NodeId>(Id); }
	inline ed::PinId  ToPinId (uint32 Id) { return static_cast<ed::PinId >(Id); }
	inline ed::LinkId ToLinkId(uint32 Id) { return static_cast<ed::LinkId>(Id); }

	inline uint32 NodeIdToU32(ed::NodeId Id) { return static_cast<uint32>(Id.Get()); }
	inline uint32 PinIdToU32 (ed::PinId  Id) { return static_cast<uint32>(Id.Get()); }
	inline uint32 LinkIdToU32(ed::LinkId Id) { return static_cast<uint32>(Id.Get()); }

	void CopyToBuffer(char* Buffer, size_t BufferSize, const FString& Value)
	{
		if (!Buffer || BufferSize == 0) return;
		std::snprintf(Buffer, BufferSize, "%s", Value.c_str());
	}

	const char* NodeTypeLabel(ELuaBlueprintNodeType Type)
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
		case ELuaBlueprintNodeType::LiteralBool:           return "Literal Bool";
		case ELuaBlueprintNodeType::LiteralInt:            return "Literal Int";
		case ELuaBlueprintNodeType::LiteralFloat:          return "Literal Float";
		case ELuaBlueprintNodeType::LiteralString:         return "Literal String";
		case ELuaBlueprintNodeType::LiteralVector:         return "Literal Vector";
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

	const char* PinTypeLabel(ELuaBlueprintPinType Type)
	{
		switch (Type)
		{
		case ELuaBlueprintPinType::Exec:   return "Exec";
		case ELuaBlueprintPinType::Bool:   return "Bool";
		case ELuaBlueprintPinType::Int:    return "Int";
		case ELuaBlueprintPinType::Float:  return "Float";
		case ELuaBlueprintPinType::String: return "String";
		case ELuaBlueprintPinType::Vector: return "Vector";
		case ELuaBlueprintPinType::Object: return "Object";
		case ELuaBlueprintPinType::Any:    return "Any";
		}
		return "Unknown";
	}

	const char* SeverityLabel(ELuaBlueprintDiagnosticSeverity Severity)
	{
		switch (Severity)
		{
		case ELuaBlueprintDiagnosticSeverity::Info:    return "Info";
		case ELuaBlueprintDiagnosticSeverity::Warning: return "Warning";
		case ELuaBlueprintDiagnosticSeverity::Error:   return "Error";
		}
		return "Unknown";
	}

	// UE Blueprint 의 카테고리별 헤더 컬러 컨벤션. 노드 분류를 한눈에 구분.
	ImVec4 NodeHeaderColor(ELuaBlueprintNodeType Type)
	{
		switch (Type)
		{
		case ELuaBlueprintNodeType::EventBeginPlay:
		case ELuaBlueprintNodeType::EventTick:
		case ELuaBlueprintNodeType::EventEndPlay:
		case ELuaBlueprintNodeType::EventOverlap:
		case ELuaBlueprintNodeType::EventEndOverlap:
		case ELuaBlueprintNodeType::EventHit:
		case ELuaBlueprintNodeType::EventEndHit:
			return ImVec4(0.95f, 0.45f, 0.45f, 1.0f);
		case ELuaBlueprintNodeType::Branch:
		case ELuaBlueprintNodeType::Sequence:
		case ELuaBlueprintNodeType::ForLoop:
		case ELuaBlueprintNodeType::WhileLoop:
			return ImVec4(0.95f, 0.80f, 0.35f, 1.0f);
		case ELuaBlueprintNodeType::CallFunction:
		case ELuaBlueprintNodeType::CallFunctionSignature:
			return ImVec4(0.45f, 0.70f, 1.00f, 1.0f);
		case ELuaBlueprintNodeType::GetVariable:
		case ELuaBlueprintNodeType::SetVariable:
		case ELuaBlueprintNodeType::GetProperty:
		case ELuaBlueprintNodeType::SetProperty:
			return ImVec4(0.55f, 0.95f, 0.65f, 1.0f);
		case ELuaBlueprintNodeType::AddFloat:
		case ELuaBlueprintNodeType::SubtractFloat:
		case ELuaBlueprintNodeType::MultiplyFloat:
		case ELuaBlueprintNodeType::DivideFloat:
		case ELuaBlueprintNodeType::AddInt:
		case ELuaBlueprintNodeType::SubtractInt:
		case ELuaBlueprintNodeType::MultiplyInt:
		case ELuaBlueprintNodeType::DivideInt:
		case ELuaBlueprintNodeType::ModInt:
		case ELuaBlueprintNodeType::AddVector:
		case ELuaBlueprintNodeType::SubtractVector:
		case ELuaBlueprintNodeType::ScaleVector:
		case ELuaBlueprintNodeType::DotVector:
		case ELuaBlueprintNodeType::CrossVector:
		case ELuaBlueprintNodeType::VectorLength:
		case ELuaBlueprintNodeType::NormalizeVector:
		case ELuaBlueprintNodeType::MakeVector:
		case ELuaBlueprintNodeType::BreakVector:
		case ELuaBlueprintNodeType::AppendString:
			return ImVec4(0.75f, 0.65f, 1.00f, 1.0f);
		case ELuaBlueprintNodeType::EqualFloat:
		case ELuaBlueprintNodeType::NotEqualFloat:
		case ELuaBlueprintNodeType::LessFloat:
		case ELuaBlueprintNodeType::GreaterFloat:
		case ELuaBlueprintNodeType::LessEqualFloat:
		case ELuaBlueprintNodeType::GreaterEqualFloat:
		case ELuaBlueprintNodeType::EqualInt:
		case ELuaBlueprintNodeType::NotEqualInt:
		case ELuaBlueprintNodeType::LessInt:
		case ELuaBlueprintNodeType::GreaterInt:
		case ELuaBlueprintNodeType::And:
		case ELuaBlueprintNodeType::Or:
		case ELuaBlueprintNodeType::Not:
			return ImVec4(1.00f, 0.55f, 0.85f, 1.0f);
		case ELuaBlueprintNodeType::Self:
			return ImVec4(0.55f, 0.85f, 0.95f, 1.0f);
		case ELuaBlueprintNodeType::SpawnActor:
		case ELuaBlueprintNodeType::DestroyActor:
		case ELuaBlueprintNodeType::FindActorByName:
		case ELuaBlueprintNodeType::FindActorByClass:
		case ELuaBlueprintNodeType::FindActorByTag:
		case ELuaBlueprintNodeType::FindActorsByTag:
		case ELuaBlueprintNodeType::FindActorsByClass:
		case ELuaBlueprintNodeType::GetActorLocation:
		case ELuaBlueprintNodeType::SetActorLocation:
		case ELuaBlueprintNodeType::GetActorRotation:
		case ELuaBlueprintNodeType::SetActorRotation:
		case ELuaBlueprintNodeType::GetActorScale:
		case ELuaBlueprintNodeType::SetActorScale:
		case ELuaBlueprintNodeType::GetActorForward:
		case ELuaBlueprintNodeType::GetActorRight:
		case ELuaBlueprintNodeType::AddActorWorldOffset:
		case ELuaBlueprintNodeType::ActorHasTag:
		case ELuaBlueprintNodeType::ActorAddTag:
		case ELuaBlueprintNodeType::ActorRemoveTag:
		case ELuaBlueprintNodeType::GetActorName:
		case ELuaBlueprintNodeType::GetOwnerActor:
			return ImVec4(0.95f, 0.55f, 0.45f, 1.0f);
		case ELuaBlueprintNodeType::IsValid:
		case ELuaBlueprintNodeType::Cast:
			return ImVec4(0.55f, 0.85f, 0.95f, 1.0f);
		case ELuaBlueprintNodeType::GetRootComponent:
		case ELuaBlueprintNodeType::GetComponentByName:
		case ELuaBlueprintNodeType::GetPrimitiveComponent:
		case ELuaBlueprintNodeType::ActivateComponent:
		case ELuaBlueprintNodeType::DeactivateComponent:
		case ELuaBlueprintNodeType::AddForce:
		case ELuaBlueprintNodeType::AddTorque:
		case ELuaBlueprintNodeType::GetLinearVelocity:
		case ELuaBlueprintNodeType::SetLinearVelocity:
		case ELuaBlueprintNodeType::GetMass:
		case ELuaBlueprintNodeType::SetSimulatePhysics:
			return ImVec4(0.45f, 0.85f, 0.65f, 1.0f);
		case ELuaBlueprintNodeType::Lerp:
		case ELuaBlueprintNodeType::Clamp:
		case ELuaBlueprintNodeType::Min:
		case ELuaBlueprintNodeType::Max:
		case ELuaBlueprintNodeType::RandomFloat:
		case ELuaBlueprintNodeType::RandomInt:
		case ELuaBlueprintNodeType::Sin:
		case ELuaBlueprintNodeType::Cos:
		case ELuaBlueprintNodeType::Sqrt:
		case ELuaBlueprintNodeType::AbsFloat:
		case ELuaBlueprintNodeType::Floor:
		case ELuaBlueprintNodeType::Ceil:
		case ELuaBlueprintNodeType::Distance:
			return ImVec4(0.75f, 0.65f, 1.00f, 1.0f);
		case ELuaBlueprintNodeType::GetGameTime:
			return ImVec4(0.95f, 0.85f, 0.40f, 1.0f);
		case ELuaBlueprintNodeType::ForEachActorByClass:
		case ELuaBlueprintNodeType::ForEachActorByTag:
			return ImVec4(0.95f, 0.80f, 0.35f, 1.0f);
		default:
			return ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
		}
	}

	// 핀 타입별 색상 — Material editor 패턴 그대로 차용해 통일성 유지.
	ImVec4 PinTypeColor(ELuaBlueprintPinType Type)
	{
		switch (Type)
		{
		case ELuaBlueprintPinType::Exec:   return ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
		case ELuaBlueprintPinType::Bool:   return ImVec4(0.95f, 0.30f, 0.30f, 1.0f);
		case ELuaBlueprintPinType::Int:    return ImVec4(0.45f, 0.95f, 0.85f, 1.0f);
		case ELuaBlueprintPinType::Float:  return ImVec4(0.55f, 0.95f, 0.45f, 1.0f);
		case ELuaBlueprintPinType::String: return ImVec4(0.95f, 0.45f, 0.85f, 1.0f);
		case ELuaBlueprintPinType::Vector: return ImVec4(0.95f, 0.85f, 0.30f, 1.0f);
		case ELuaBlueprintPinType::Object: return ImVec4(0.40f, 0.75f, 1.00f, 1.0f);
		case ELuaBlueprintPinType::Any:    return ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
		}
		return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
	}

	bool IsEventNode(ELuaBlueprintNodeType Type)
	{
		switch (Type)
		{
		case ELuaBlueprintNodeType::EventBeginPlay:
		case ELuaBlueprintNodeType::EventTick:
		case ELuaBlueprintNodeType::EventEndPlay:
		case ELuaBlueprintNodeType::EventOverlap:
		case ELuaBlueprintNodeType::EventEndOverlap:
		case ELuaBlueprintNodeType::EventHit:
		case ELuaBlueprintNodeType::EventEndHit:
			return true;
		default:
			return false;
		}
	}

	bool HasNodeOfType(const ULuaBlueprintAsset* Blueprint, ELuaBlueprintNodeType Type)
	{
		if (!Blueprint) return false;
		for (const FLuaBlueprintNode& Node : Blueprint->GetNodes())
		{
			if (Node.Type == Type) return true;
		}
		return false;
	}

	bool ContainsCaseInsensitive(const char* Haystack, const char* Needle)
	{
		if (!Needle || !*Needle) return true;
		if (!Haystack) return false;
		const size_t HN = std::strlen(Haystack);
		const size_t NN = std::strlen(Needle);
		if (NN > HN) return false;
		for (size_t i = 0; i + NN <= HN; ++i)
		{
			bool bMatch = true;
			for (size_t j = 0; j < NN; ++j)
			{
				if (std::tolower(static_cast<unsigned char>(Haystack[i + j])) !=
					std::tolower(static_cast<unsigned char>(Needle[j])))
				{
					bMatch = false;
					break;
				}
			}
			if (bMatch) return true;
		}
		return false;
	}
}

FLuaBlueprintEditorWidget::~FLuaBlueprintEditorWidget()
{
	DestroyContext();
}

bool FLuaBlueprintEditorWidget::CanEdit(UObject* Object) const
{
	return Cast<ULuaBlueprintAsset>(Object) != nullptr;
}

void FLuaBlueprintEditorWidget::Open(UObject* Object)
{
	if (!CanEdit(Object)) return;

	FAssetEditorWidget::Open(Object);
	EnsureContext();
	bPositionsPushed = false;

	if (ULuaBlueprintAsset* Blueprint = GetBlueprint())
	{
		if (Blueprint->GetNodes().empty())
		{
			Blueprint->InitializeDefault();
		}
		else if (Blueprint->IsCompileDirty())
		{
			Blueprint->Compile();
		}
	}
}

void FLuaBlueprintEditorWidget::Close()
{
	DestroyContext();
	bPositionsPushed = false;
	FAssetEditorWidget::Close();
}

void FLuaBlueprintEditorWidget::EnsureContext()
{
	if (NodeEditorContext) return;
	ed::Config Cfg;
	Cfg.SettingsFile = nullptr;
	NodeEditorContext = ed::CreateEditor(&Cfg);
}

void FLuaBlueprintEditorWidget::DestroyContext()
{
	if (NodeEditorContext)
	{
		ed::DestroyEditor(NodeEditorContext);
		NodeEditorContext = nullptr;
	}
}

void FLuaBlueprintEditorWidget::Render(float /*DeltaTime*/)
{
	ULuaBlueprintAsset* Blueprint = GetBlueprint();
	if (!Blueprint)
	{
		Close();
		return;
	}

	EnsureContext();

	FString WindowTitle = FString("Lua Blueprint - ") + (Blueprint->GetSourcePath().empty() ? Blueprint->GetName() : Blueprint->GetSourcePath());
	if (IsDirty() || Blueprint->IsCompileDirty()) WindowTitle += "*";

	bool bWindowOpen = IsOpen();
	if (!ImGui::Begin(WindowTitle.c_str(), &bWindowOpen, ImGuiWindowFlags_MenuBar))
	{
		ImGui::End();
		if (!bWindowOpen) Close();
		return;
	}

	if (ConsumeFocusRequest())
	{
		ImGui::SetWindowFocus();
	}

	RenderToolbar(Blueprint);
	RenderCompileErrorPanel(Blueprint);

	const float BottomHeight = 180.0f;
	ImGui::BeginChild("##LuaBlueprintMainArea", ImVec2(0, -BottomHeight), ImGuiChildFlags_None);
	RenderVariables(Blueprint);
	ImGui::SameLine();
	RenderGraph(Blueprint);
	ImGui::EndChild();

	ImGui::Separator();
	if (ImGui::BeginTabBar("##LuaBlueprintBottomTabs"))
	{
		if (ImGui::BeginTabItem("Diagnostics"))
		{
			RenderDiagnostics(Blueprint);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Generated Lua"))
		{
			RenderGeneratedLua(Blueprint);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
	if (!bWindowOpen) Close();
}

ULuaBlueprintAsset* FLuaBlueprintEditorWidget::GetBlueprint() const
{
	return Cast<ULuaBlueprintAsset>(EditedObject);
}

void FLuaBlueprintEditorWidget::RenderToolbar(ULuaBlueprintAsset* Blueprint)
{
	if (!ImGui::BeginMenuBar()) return;

	const bool bDirtyNow = IsDirty() || Blueprint->IsCompileDirty();
	// dirty 면 Save 버튼 강조 — Material editor 패턴.
	if (bDirtyNow) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.85f, 0.35f, 1.0f));
	if (ImGui::MenuItem(bDirtyNow ? "Save*" : "Save"))
	{
		if (Blueprint->Compile() && FLuaBlueprintManager::Get().Save(Blueprint))
		{
			ClearDirty();
		}
	}
	if (bDirtyNow) ImGui::PopStyleColor();

	if (ImGui::MenuItem("Compile"))
	{
		Blueprint->Compile();
	}

	if (ImGui::MenuItem("Reset Default"))
	{
		Blueprint->InitializeDefault();
		bPositionsPushed = false;
		MarkDirty();
	}
	ImGui::Separator();
	ImGui::TextDisabled("Right-click canvas for menu. Drag pins to link. Drag a variable to canvas for Get/Set.");

	ImGui::EndMenuBar();
}

void FLuaBlueprintEditorWidget::RenderCompileErrorPanel(ULuaBlueprintAsset* Blueprint)
{
	// Material 패턴: 상단에 빨간 에러 패널을 명시적으로 노출. Diagnostics 탭은 보조용.
	bool bHasError = false;
	int  NumWarnings = 0;
	for (const FLuaBlueprintDiagnostic& D : Blueprint->GetDiagnostics())
	{
		if (D.Severity == ELuaBlueprintDiagnosticSeverity::Error)   bHasError = true;
		if (D.Severity == ELuaBlueprintDiagnosticSeverity::Warning) ++NumWarnings;
	}

	if (!bHasError && NumWarnings == 0) return;

	const ImVec4 Bg = bHasError ? ImVec4(0.25f, 0.10f, 0.10f, 0.6f) : ImVec4(0.25f, 0.20f, 0.10f, 0.6f);
	ImGui::PushStyleColor(ImGuiCol_ChildBg, Bg);
	const float Height = bHasError ? 80.0f : 50.0f;
	ImGui::BeginChild("##LuaBlueprintCompileBanner", ImVec2(0, Height), ImGuiChildFlags_Borders);

	if (bHasError)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Compile errors:");
		for (const FLuaBlueprintDiagnostic& D : Blueprint->GetDiagnostics())
		{
			if (D.Severity != ELuaBlueprintDiagnosticSeverity::Error) continue;
			ImGui::BulletText("Node %u: %s", D.NodeId, D.Message.c_str());
		}
	}
	else
	{
		ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.35f, 1.0f), "Compile warnings: %d (Diagnostics 탭 참고)", NumWarnings);
	}

	ImGui::EndChild();
	ImGui::PopStyleColor();
}

bool FLuaBlueprintEditorWidget::AddVariableMenuItem(ULuaBlueprintAsset* Blueprint, ELuaBlueprintPinType Type, const char* Label)
{
	if (!ImGui::MenuItem(Label)) return false;

	char NameBuf[48];
	std::snprintf(NameBuf, sizeof(NameBuf), "Var%u", static_cast<uint32>(Blueprint->GetVariables().size()));
	Blueprint->AddVariable(FName(NameBuf), Type);
	MarkDirty();
	return true;
}

void FLuaBlueprintEditorWidget::RenderVariables(ULuaBlueprintAsset* Blueprint)
{
	const float Width = 260.0f;
	ImGui::BeginChild("##LuaBlueprintVariables", ImVec2(Width, 0), ImGuiChildFlags_Borders);

	ImGui::TextUnformatted("Variables");
	ImGui::SameLine();
	if (ImGui::SmallButton("+"))
	{
		ImGui::OpenPopup("LuaBlueprintAddVariableMenu");
	}

	if (ImGui::BeginPopup("LuaBlueprintAddVariableMenu"))
	{
		AddVariableMenuItem(Blueprint, ELuaBlueprintPinType::Bool, "Bool");
		AddVariableMenuItem(Blueprint, ELuaBlueprintPinType::Int, "Int");
		AddVariableMenuItem(Blueprint, ELuaBlueprintPinType::Float, "Float");
		AddVariableMenuItem(Blueprint, ELuaBlueprintPinType::String, "String");
		AddVariableMenuItem(Blueprint, ELuaBlueprintPinType::Vector, "Vector");
		AddVariableMenuItem(Blueprint, ELuaBlueprintPinType::Object, "Object");
		ImGui::EndPopup();
	}

	ImGui::Separator();

	TArray<FLuaBlueprintVariable>& Variables = Blueprint->GetMutableVariables();
	for (int32 Index = 0; Index < static_cast<int32>(Variables.size()); ++Index)
	{
		FLuaBlueprintVariable& Variable = Variables[Index];
		ImGui::PushID(Index);

		const ImVec4 TypeColor = PinTypeColor(Variable.Type);
		// 행 헤더: 타입 색상 + 이름. drag source — 캔버스로 끌어다 놓으면 Get/Set 선택.
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
		const FString HeaderLabel = FString(Variable.Name.ToString()) + " : " + PinTypeLabel(Variable.Type);
		const bool bOpen = ImGui::TreeNodeEx("##Var", ImGuiTreeNodeFlags_DefaultOpen, " ");
		ImGui::PopStyleColor();
		ImGui::SameLine();
		ImGui::TextColored(TypeColor, "%s", HeaderLabel.c_str());

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
		{
			const FName DragName = Variable.Name;
			ImGui::SetDragDropPayload("LuaBlueprintVariable", &DragName, sizeof(FName));
			ImGui::TextColored(TypeColor, "%s", HeaderLabel.c_str());
			ImGui::EndDragDropSource();
		}

		if (bOpen)
		{
			RenderVariableEditor(Blueprint, Variable, Index);
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	ImGui::EndChild();
}

void FLuaBlueprintEditorWidget::RenderVariableEditor(ULuaBlueprintAsset* Blueprint, FLuaBlueprintVariable& Variable, int32 Index)
{
	char NameBuf[128];
	CopyToBuffer(NameBuf, sizeof(NameBuf), Variable.Name.ToString());
	if (ImGui::InputText("Name", NameBuf, sizeof(NameBuf), ImGuiInputTextFlags_EnterReturnsTrue))
	{
		const FName OldName = Variable.Name;
		const FName NewName = NameBuf[0] ? FName(NameBuf) : FName::None;
		Variable.Name = NewName;
		// 이 변수를 참조하는 Get/Set 노드의 NameValue 도 함께 갱신 — AnimGraph state rename 패턴.
		RenameVariableCascade(Blueprint, OldName, NewName);
		Blueprint->BumpVersion();
		MarkDirty();
	}

	int TypeIndex = static_cast<int>(Variable.Type);
	const char* TypeLabels[] = { "Exec", "Bool", "Int", "Float", "String", "Vector", "Object", "Any" };
	if (ImGui::Combo("Type", &TypeIndex, TypeLabels, IM_ARRAYSIZE(TypeLabels)))
	{
		if (TypeIndex == static_cast<int>(ELuaBlueprintPinType::Exec) || TypeIndex == static_cast<int>(ELuaBlueprintPinType::Any))
		{
			TypeIndex = static_cast<int>(ELuaBlueprintPinType::Float);
		}
		Variable.Type = static_cast<ELuaBlueprintPinType>(TypeIndex);
		Blueprint->BumpVersion();
		MarkDirty();
	}

	switch (Variable.Type)
	{
	case ELuaBlueprintPinType::Bool:
		if (ImGui::Checkbox("Default", &Variable.BoolValue)) { Blueprint->BumpVersion(); MarkDirty(); }
		break;
	case ELuaBlueprintPinType::Int:
		if (ImGui::InputInt("Default", &Variable.IntValue)) { Blueprint->BumpVersion(); MarkDirty(); }
		break;
	case ELuaBlueprintPinType::Float:
		if (ImGui::InputFloat("Default", &Variable.FloatValue)) { Blueprint->BumpVersion(); MarkDirty(); }
		break;
	case ELuaBlueprintPinType::String:
	{
		char ValueBuf[512];
		CopyToBuffer(ValueBuf, sizeof(ValueBuf), Variable.StringValue);
		if (ImGui::InputText("Default", ValueBuf, sizeof(ValueBuf)))
		{
			Variable.StringValue = ValueBuf;
			Blueprint->BumpVersion();
			MarkDirty();
		}
		break;
	}
	case ELuaBlueprintPinType::Vector:
	{
		float V[3] = { Variable.VectorValue.X, Variable.VectorValue.Y, Variable.VectorValue.Z };
		if (ImGui::InputFloat3("Default", V))
		{
			Variable.VectorValue = FVector(V[0], V[1], V[2]);
			Blueprint->BumpVersion();
			MarkDirty();
		}
		break;
	}
	case ELuaBlueprintPinType::Object:
		if (ImGui::Checkbox("Strong Object Ref", &Variable.bStrongObject)) { Blueprint->BumpVersion(); MarkDirty(); }
		break;
	default:
		break;
	}

	// Get/Set 빠른 추가 버튼 — 실제 BP 의 ctrl/alt+drag 대안. PendingNewNodePosition 가 비어있으면
	// 캔버스 우상단 근처 기본 위치로.
	const ImVec2 SpawnPos = (PendingNewNodePosition.x != 0.0f || PendingNewNodePosition.y != 0.0f)
		? PendingNewNodePosition
		: ImVec2(100.0f + Index * 30.0f, 100.0f + Index * 30.0f);

	if (ImGui::SmallButton("Get"))
	{
		SpawnVariableNode(Blueprint, ELuaBlueprintNodeType::GetVariable, Variable.Name, SpawnPos);
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Set"))
	{
		SpawnVariableNode(Blueprint, ELuaBlueprintNodeType::SetVariable, Variable.Name, SpawnPos);
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Remove"))
	{
		TArray<FLuaBlueprintVariable>& Vars = Blueprint->GetMutableVariables();
		if (Index >= 0 && Index < static_cast<int32>(Vars.size()))
		{
			Vars.erase(Vars.begin() + Index);
			Blueprint->BumpVersion();
			MarkDirty();
		}
	}
}

void FLuaBlueprintEditorWidget::SpawnVariableNode(ULuaBlueprintAsset* Blueprint, ELuaBlueprintNodeType Type, const FName& VariableName, const ImVec2& Position)
{
	if (Type != ELuaBlueprintNodeType::GetVariable && Type != ELuaBlueprintNodeType::SetVariable) return;

	FLuaBlueprintNode* NewNode = Blueprint->AddNodeOfType(Type, Position.x, Position.y);
	if (!NewNode) return;

	NewNode->NameValue = VariableName;
	if (NodeEditorContext)
	{
		// 호출 context 가 두 가지: ① RenderGraph 의 Suspend 블록 안 (drop popup) — 이미 ed 가 current,
		// ② RenderVariables 의 Get/Set 버튼 — ed 가 not current.
		// 임의로 SetCurrentEditor(nullptr) 하면 ① 에서 바깥 Resume() 이 null context 로 터진다.
		// → "이미 current 이면 그대로 사용, 아니면 잠깐 set 후 복구" 패턴.
		ed::EditorContext* Prev = ed::GetCurrentEditor();
		if (Prev != NodeEditorContext) ed::SetCurrentEditor(NodeEditorContext);
		ed::SetNodePosition(ToNodeId(NewNode->NodeId), Position);
		if (Prev != NodeEditorContext) ed::SetCurrentEditor(Prev);
	}
	Blueprint->BumpVersion();
	MarkDirty();
}

void FLuaBlueprintEditorWidget::RenameVariableCascade(ULuaBlueprintAsset* Blueprint, const FName& OldName, const FName& NewName)
{
	if (OldName == NewName) return;
	for (FLuaBlueprintNode& Node : Blueprint->GetMutableNodes())
	{
		if (Node.Type != ELuaBlueprintNodeType::GetVariable && Node.Type != ELuaBlueprintNodeType::SetVariable) continue;
		if (Node.NameValue == OldName) Node.NameValue = NewName;
	}
}

bool FLuaBlueprintEditorWidget::AddNodeMenuItem(ULuaBlueprintAsset* Blueprint, ELuaBlueprintNodeType Type)
{
	const bool bDisabled = IsEventNode(Type) && HasNodeOfType(Blueprint, Type);
	if (bDisabled) ImGui::BeginDisabled();
	ImGui::PushStyleColor(ImGuiCol_Text, NodeHeaderColor(Type));
	const bool bClicked = ImGui::MenuItem(NodeTypeLabel(Type));
	ImGui::PopStyleColor();
	if (bClicked)
	{
		FLuaBlueprintNode* NewNode = Blueprint->AddNodeOfType(Type, PendingNewNodePosition.x, PendingNewNodePosition.y);
		if (NewNode && NodeEditorContext)
		{
			ed::SetNodePosition(ToNodeId(NewNode->NodeId), PendingNewNodePosition);
		}
		MarkDirty();
	}
	if (bDisabled) ImGui::EndDisabled();
	return bClicked;
}

void FLuaBlueprintEditorWidget::RenderAddNodeMenu(ULuaBlueprintAsset* Blueprint)
{
	ImGui::TextDisabled("Add Node");
	ImGui::Separator();
	ImGui::SetNextItemWidth(220.0f);
	ImGui::InputTextWithHint("##Search", "search...", AddNodeSearchBuf, sizeof(AddNodeSearchBuf));
	const char* Query = AddNodeSearchBuf;

	auto AddItem = [&](ELuaBlueprintNodeType Type)
	{
		if (!ContainsCaseInsensitive(NodeTypeLabel(Type), Query)) return;
		AddNodeMenuItem(Blueprint, Type);
	};

	const bool bHasQuery = Query[0] != 0;
	if (bHasQuery)
	{
		// 검색 모드: 카테고리 안 펼치고 한 줄로.
		for (int32 i = 0; i <= static_cast<int32>(ELuaBlueprintNodeType::ForEachActorByTag); ++i)
		{
			AddItem(static_cast<ELuaBlueprintNodeType>(i));
		}
	}
	else
	{
		if (ImGui::BeginMenu("Events"))
		{
			AddItem(ELuaBlueprintNodeType::EventBeginPlay);
			AddItem(ELuaBlueprintNodeType::EventTick);
			AddItem(ELuaBlueprintNodeType::EventEndPlay);
			AddItem(ELuaBlueprintNodeType::EventOverlap);
			AddItem(ELuaBlueprintNodeType::EventEndOverlap);
			AddItem(ELuaBlueprintNodeType::EventHit);
			AddItem(ELuaBlueprintNodeType::EventEndHit);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Flow"))
		{
			AddItem(ELuaBlueprintNodeType::Sequence);
			AddItem(ELuaBlueprintNodeType::Branch);
			AddItem(ELuaBlueprintNodeType::ForLoop);
			AddItem(ELuaBlueprintNodeType::WhileLoop);
			AddItem(ELuaBlueprintNodeType::ForEachActorByClass);
			AddItem(ELuaBlueprintNodeType::ForEachActorByTag);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Actor"))
		{
			AddItem(ELuaBlueprintNodeType::SpawnActor);
			AddItem(ELuaBlueprintNodeType::DestroyActor);
			AddItem(ELuaBlueprintNodeType::FindActorByName);
			AddItem(ELuaBlueprintNodeType::FindActorByClass);
			AddItem(ELuaBlueprintNodeType::FindActorByTag);
			AddItem(ELuaBlueprintNodeType::FindActorsByTag);
			AddItem(ELuaBlueprintNodeType::FindActorsByClass);
			ImGui::Separator();
			AddItem(ELuaBlueprintNodeType::GetActorLocation);
			AddItem(ELuaBlueprintNodeType::SetActorLocation);
			AddItem(ELuaBlueprintNodeType::GetActorRotation);
			AddItem(ELuaBlueprintNodeType::SetActorRotation);
			AddItem(ELuaBlueprintNodeType::GetActorScale);
			AddItem(ELuaBlueprintNodeType::SetActorScale);
			AddItem(ELuaBlueprintNodeType::GetActorForward);
			AddItem(ELuaBlueprintNodeType::GetActorRight);
			AddItem(ELuaBlueprintNodeType::AddActorWorldOffset);
			ImGui::Separator();
			AddItem(ELuaBlueprintNodeType::ActorHasTag);
			AddItem(ELuaBlueprintNodeType::ActorAddTag);
			AddItem(ELuaBlueprintNodeType::ActorRemoveTag);
			AddItem(ELuaBlueprintNodeType::GetActorName);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Object"))
		{
			AddItem(ELuaBlueprintNodeType::IsValid);
			AddItem(ELuaBlueprintNodeType::Cast);
			AddItem(ELuaBlueprintNodeType::GetOwnerActor);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Component"))
		{
			AddItem(ELuaBlueprintNodeType::GetRootComponent);
			AddItem(ELuaBlueprintNodeType::GetComponentByName);
			AddItem(ELuaBlueprintNodeType::GetPrimitiveComponent);
			AddItem(ELuaBlueprintNodeType::ActivateComponent);
			AddItem(ELuaBlueprintNodeType::DeactivateComponent);
			ImGui::Separator();
			AddItem(ELuaBlueprintNodeType::AddForce);
			AddItem(ELuaBlueprintNodeType::AddTorque);
			AddItem(ELuaBlueprintNodeType::GetLinearVelocity);
			AddItem(ELuaBlueprintNodeType::SetLinearVelocity);
			AddItem(ELuaBlueprintNodeType::GetMass);
			AddItem(ELuaBlueprintNodeType::SetSimulatePhysics);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Utility"))
		{
			AddItem(ELuaBlueprintNodeType::Lerp);
			AddItem(ELuaBlueprintNodeType::Clamp);
			AddItem(ELuaBlueprintNodeType::Min);
			AddItem(ELuaBlueprintNodeType::Max);
			AddItem(ELuaBlueprintNodeType::RandomFloat);
			AddItem(ELuaBlueprintNodeType::RandomInt);
			AddItem(ELuaBlueprintNodeType::Sin);
			AddItem(ELuaBlueprintNodeType::Cos);
			AddItem(ELuaBlueprintNodeType::Sqrt);
			AddItem(ELuaBlueprintNodeType::AbsFloat);
			AddItem(ELuaBlueprintNodeType::Floor);
			AddItem(ELuaBlueprintNodeType::Ceil);
			AddItem(ELuaBlueprintNodeType::Distance);
			AddItem(ELuaBlueprintNodeType::GetGameTime);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Variables / Properties"))
		{
			AddItem(ELuaBlueprintNodeType::GetVariable);
			AddItem(ELuaBlueprintNodeType::SetVariable);
			AddItem(ELuaBlueprintNodeType::GetProperty);
			AddItem(ELuaBlueprintNodeType::SetProperty);
			AddItem(ELuaBlueprintNodeType::Self);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Functions"))
		{
			AddItem(ELuaBlueprintNodeType::CallFunction);
			AddItem(ELuaBlueprintNodeType::CallFunctionSignature);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Literals"))
		{
			AddItem(ELuaBlueprintNodeType::LiteralBool);
			AddItem(ELuaBlueprintNodeType::LiteralInt);
			AddItem(ELuaBlueprintNodeType::LiteralFloat);
			AddItem(ELuaBlueprintNodeType::LiteralString);
			AddItem(ELuaBlueprintNodeType::LiteralVector);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Math (Float)"))
		{
			AddItem(ELuaBlueprintNodeType::AddFloat);
			AddItem(ELuaBlueprintNodeType::SubtractFloat);
			AddItem(ELuaBlueprintNodeType::MultiplyFloat);
			AddItem(ELuaBlueprintNodeType::DivideFloat);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Math (Int)"))
		{
			AddItem(ELuaBlueprintNodeType::AddInt);
			AddItem(ELuaBlueprintNodeType::SubtractInt);
			AddItem(ELuaBlueprintNodeType::MultiplyInt);
			AddItem(ELuaBlueprintNodeType::DivideInt);
			AddItem(ELuaBlueprintNodeType::ModInt);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Compare"))
		{
			AddItem(ELuaBlueprintNodeType::EqualFloat);
			AddItem(ELuaBlueprintNodeType::NotEqualFloat);
			AddItem(ELuaBlueprintNodeType::LessFloat);
			AddItem(ELuaBlueprintNodeType::GreaterFloat);
			AddItem(ELuaBlueprintNodeType::LessEqualFloat);
			AddItem(ELuaBlueprintNodeType::GreaterEqualFloat);
			AddItem(ELuaBlueprintNodeType::EqualInt);
			AddItem(ELuaBlueprintNodeType::NotEqualInt);
			AddItem(ELuaBlueprintNodeType::LessInt);
			AddItem(ELuaBlueprintNodeType::GreaterInt);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Logic"))
		{
			AddItem(ELuaBlueprintNodeType::And);
			AddItem(ELuaBlueprintNodeType::Or);
			AddItem(ELuaBlueprintNodeType::Not);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("String"))
		{
			AddItem(ELuaBlueprintNodeType::AppendString);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Vector"))
		{
			AddItem(ELuaBlueprintNodeType::MakeVector);
			AddItem(ELuaBlueprintNodeType::BreakVector);
			AddItem(ELuaBlueprintNodeType::AddVector);
			AddItem(ELuaBlueprintNodeType::SubtractVector);
			AddItem(ELuaBlueprintNodeType::ScaleVector);
			AddItem(ELuaBlueprintNodeType::DotVector);
			AddItem(ELuaBlueprintNodeType::CrossVector);
			AddItem(ELuaBlueprintNodeType::VectorLength);
			AddItem(ELuaBlueprintNodeType::NormalizeVector);
			ImGui::EndMenu();
		}
		ImGui::Separator();
		AddItem(ELuaBlueprintNodeType::PrintString);
	}
}

bool FLuaBlueprintEditorWidget::RenderInlinePinLiteral(ULuaBlueprintAsset* Blueprint, FLuaBlueprintNode& /*Node*/, FLuaBlueprintPin& Pin)
{
	// 데이터 input pin 만 inline editor 노출. Exec / Object / Any 는 inline literal 의미가 없음.
	if (Pin.Kind != ELuaBlueprintPinKind::Input) return false;
	if (Pin.Type == ELuaBlueprintPinType::Exec || Pin.Type == ELuaBlueprintPinType::Object || Pin.Type == ELuaBlueprintPinType::Any) return false;

	// 이미 link 가 있으면 inline literal 숨김 (실제 Blueprint 와 동일).
	if (Blueprint->FindLinkToInput(Pin.PinId) != nullptr) return false;

	bool bChanged = false;
	ImGui::PushID(static_cast<int>(Pin.PinId));
	switch (Pin.Type)
	{
	case ELuaBlueprintPinType::Bool:
		if (ImGui::Checkbox("##def", &Pin.DefaultBool)) bChanged = true;
		break;
	case ELuaBlueprintPinType::Int:
		ImGui::SetNextItemWidth(80.0f);
		if (ImGui::InputInt("##def", &Pin.DefaultInt, 0)) bChanged = true;
		break;
	case ELuaBlueprintPinType::Float:
		ImGui::SetNextItemWidth(80.0f);
		if (ImGui::DragFloat("##def", &Pin.DefaultFloat, 0.01f, 0.0f, 0.0f, "%.3f")) bChanged = true;
		break;
	case ELuaBlueprintPinType::String:
	{
		char Buf[256];
		CopyToBuffer(Buf, sizeof(Buf), Pin.DefaultString);
		ImGui::SetNextItemWidth(160.0f);
		if (ImGui::InputText("##def", Buf, sizeof(Buf)))
		{
			Pin.DefaultString = Buf;
			bChanged = true;
		}
		break;
	}
	case ELuaBlueprintPinType::Vector:
	{
		float V[3] = { Pin.DefaultVector.X, Pin.DefaultVector.Y, Pin.DefaultVector.Z };
		ImGui::SetNextItemWidth(160.0f);
		if (ImGui::DragFloat3("##def", V, 0.01f))
		{
			Pin.DefaultVector = FVector(V[0], V[1], V[2]);
			bChanged = true;
		}
		break;
	}
	default:
		break;
	}
	ImGui::PopID();

	if (bChanged)
	{
		Blueprint->BumpVersion();
		MarkDirty();
	}
	return bChanged;
}

void FLuaBlueprintEditorWidget::RenderNodeBody(ULuaBlueprintAsset* Blueprint, FLuaBlueprintNode& Node)
{
	// Material editor 가 노드 본문에 색 swatch / 텍스처 썸네일을 그리는 것과 같은 자리.
	// LuaBP literal 노드는 인라인으로 값 편집기를 표시 — 노드 안에서 바로 수정 가능.
	switch (Node.Type)
	{
	case ELuaBlueprintNodeType::LiteralBool:
		ImGui::PushID(static_cast<int>(Node.NodeId));
		if (ImGui::Checkbox("##lb", &Node.BoolValue)) { Blueprint->BumpVersion(); MarkDirty(); }
		ImGui::PopID();
		break;
	case ELuaBlueprintNodeType::LiteralInt:
		ImGui::PushID(static_cast<int>(Node.NodeId));
		ImGui::SetNextItemWidth(80.0f);
		if (ImGui::InputInt("##li", &Node.IntValue, 0)) { Blueprint->BumpVersion(); MarkDirty(); }
		ImGui::PopID();
		break;
	case ELuaBlueprintNodeType::LiteralFloat:
		ImGui::PushID(static_cast<int>(Node.NodeId));
		ImGui::SetNextItemWidth(80.0f);
		if (ImGui::DragFloat("##lf", &Node.FloatValue, 0.01f, 0.0f, 0.0f, "%.3f")) { Blueprint->BumpVersion(); MarkDirty(); }
		ImGui::PopID();
		break;
	case ELuaBlueprintNodeType::LiteralString:
	{
		ImGui::PushID(static_cast<int>(Node.NodeId));
		char Buf[256];
		CopyToBuffer(Buf, sizeof(Buf), Node.StringValue);
		ImGui::SetNextItemWidth(180.0f);
		if (ImGui::InputText("##ls", Buf, sizeof(Buf)))
		{
			Node.StringValue = Buf;
			Blueprint->BumpVersion();
			MarkDirty();
		}
		ImGui::PopID();
		break;
	}
	case ELuaBlueprintNodeType::LiteralVector:
	{
		ImGui::PushID(static_cast<int>(Node.NodeId));
		float V[3] = { Node.VectorValue.X, Node.VectorValue.Y, Node.VectorValue.Z };
		ImGui::SetNextItemWidth(180.0f);
		if (ImGui::DragFloat3("##lv", V, 0.01f))
		{
			Node.VectorValue = FVector(V[0], V[1], V[2]);
			Blueprint->BumpVersion();
			MarkDirty();
		}
		ImGui::PopID();
		break;
	}
	case ELuaBlueprintNodeType::GetVariable:
	case ELuaBlueprintNodeType::SetVariable:
		if (Node.NameValue != FName::None)
		{
			ImGui::TextDisabled("[%s]", Node.NameValue.ToString().c_str());
		}
		break;
	case ELuaBlueprintNodeType::GetProperty:
	case ELuaBlueprintNodeType::SetProperty:
	case ELuaBlueprintNodeType::CallFunction:
		if (Node.NameValue != FName::None)
		{
			ImGui::TextDisabled(".%s", Node.NameValue.ToString().c_str());
		}
		break;
	case ELuaBlueprintNodeType::CallFunctionSignature:
		if (!Node.StringValue.empty())
		{
			ImGui::TextDisabled("%s", Node.StringValue.c_str());
		}
		break;
	case ELuaBlueprintNodeType::SpawnActor:
	case ELuaBlueprintNodeType::FindActorByClass:
	case ELuaBlueprintNodeType::FindActorsByClass:
	case ELuaBlueprintNodeType::Cast:
	case ELuaBlueprintNodeType::ForEachActorByClass:
		ImGui::TextDisabled(Node.NameValue == FName::None ? "(no class)" : Node.NameValue.ToString().c_str());
		break;
	case ELuaBlueprintNodeType::ForEachActorByTag:
		ImGui::TextDisabled(Node.StringValue.empty() ? "(no tag)" : Node.StringValue.c_str());
		break;
	default:
		break;
	}
}

void FLuaBlueprintEditorWidget::RenderGraph(ULuaBlueprintAsset* Blueprint)
{
	const float InspectorWidth = 360.0f;
	const float Spacing = ImGui::GetStyle().ItemSpacing.x;
	const float TotalWidth = ImGui::GetContentRegionAvail().x;
	const float CanvasWidth = (TotalWidth > InspectorWidth + Spacing + 120.0f)
		? TotalWidth - InspectorWidth - Spacing
		: TotalWidth;

	uint32 SelectedNodeId = 0;

	ImGui::BeginChild("##LuaBlueprintCanvasChild", ImVec2(CanvasWidth, 0), ImGuiChildFlags_Borders);

	ed::SetCurrentEditor(NodeEditorContext);
	ed::Begin("LuaBlueprintCanvas");

	// 이전 프레임에 변수 drop 이 들어왔다면 이제 ed 컨텍스트가 활성 상태이므로
	// 안전하게 screen→canvas 변환을 끝내고 Get/Set 팝업을 띄울 준비.
	if (bPendingVariableDrop)
	{
		PendingVariableDropPos = ed::ScreenToCanvas(PendingVariableScreenPos);
		bPendingVariableDrop = false;
		bShowVariableDropMenu = true;
	}

	if (!bPositionsPushed)
	{
		for (const FLuaBlueprintNode& Node : Blueprint->GetNodes())
		{
			ed::SetNodePosition(ToNodeId(Node.NodeId), ImVec2(Node.PosX, Node.PosY));
		}
		bPositionsPushed = true;
	}

	for (FLuaBlueprintNode& Node : Blueprint->GetMutableNodes())
	{
		ed::BeginNode(ToNodeId(Node.NodeId));
		ImGui::TextColored(NodeHeaderColor(Node.Type), "%s", NodeTypeLabel(Node.Type));
		ImGui::Dummy(ImVec2(0.0f, 2.0f));

		RenderNodeBody(Blueprint, Node);

		for (FLuaBlueprintPin& Pin : Node.Pins)
		{
			ed::BeginPin(ToPinId(Pin.PinId), Pin.Kind == ELuaBlueprintPinKind::Input ? ed::PinKind::Input : ed::PinKind::Output);
			const ImVec4 PinCol = PinTypeColor(Pin.Type);
			if (Pin.Kind == ELuaBlueprintPinKind::Input)
			{
				ImGui::TextColored(PinCol, "-> %s", Pin.DisplayName.ToString().c_str());
			}
			else
			{
				ImGui::TextColored(PinCol, "%s ->", Pin.DisplayName.ToString().c_str());
			}
			ed::EndPin();

			// Input pin 옆에 inline literal editor — 실제 Blueprint 의 핵심 UX.
			if (Pin.Kind == ELuaBlueprintPinKind::Input)
			{
				ImGui::SameLine();
				RenderInlinePinLiteral(Blueprint, Node, Pin);
			}
		}
		ImGui::Dummy(ImVec2(0.0f, 2.0f));
		ed::EndNode();
	}

	for (const FLuaBlueprintLink& Link : Blueprint->GetLinks())
	{
		// 링크 색상 = output pin 타입 색상. 데이터 흐름 가독성 강화.
		ImVec4 LinkColor(0.8f, 0.8f, 0.8f, 1.0f);
		if (const FLuaBlueprintPin* From = Blueprint->FindPin(Link.FromPinId))
		{
			LinkColor = PinTypeColor(From->Type);
		}
		ed::Link(ToLinkId(Link.LinkId), ToPinId(Link.FromPinId), ToPinId(Link.ToPinId), LinkColor);
	}

	if (ed::BeginCreate())
	{
		ed::PinId StartId, EndId;
		if (ed::QueryNewLink(&StartId, &EndId))
		{
			if (StartId && EndId)
			{
				uint32 FromPinId = 0;
				uint32 ToPinIdValue = 0;
				if (Blueprint->CanLinkPins(PinIdToU32(StartId), PinIdToU32(EndId), &FromPinId, &ToPinIdValue))
				{
					if (ed::AcceptNewItem())
					{
						Blueprint->AddLink(FromPinId, ToPinIdValue);
						MarkDirty();
					}
				}
				else
				{
					ed::RejectNewItem(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), 2.0f);
				}
			}
		}
	}
	ed::EndCreate();

	if (ed::BeginDelete())
	{
		ed::LinkId DeletedLink;
		while (ed::QueryDeletedLink(&DeletedLink))
		{
			if (ed::AcceptDeletedItem())
			{
				if (Blueprint->RemoveLink(LinkIdToU32(DeletedLink))) MarkDirty();
			}
		}

		ed::NodeId DeletedNode;
		while (ed::QueryDeletedNode(&DeletedNode))
		{
			if (ed::AcceptDeletedItem())
			{
				if (Blueprint->RemoveNode(NodeIdToU32(DeletedNode))) MarkDirty();
			}
		}
	}
	ed::EndDelete();

	for (FLuaBlueprintNode& Node : Blueprint->GetMutableNodes())
	{
		const ImVec2 Pos = ed::GetNodePosition(ToNodeId(Node.NodeId));
		if (std::fabs(Node.PosX - Pos.x) > 0.01f || std::fabs(Node.PosY - Pos.y) > 0.01f)
		{
			Node.PosX = Pos.x;
			Node.PosY = Pos.y;
			MarkDirty();
		}
	}

	ed::NodeId ContextNodeId = 0;
	ed::PinId ContextPinId = 0;
	ed::LinkId ContextLinkId = 0;

	ed::Suspend();
	if (ed::ShowNodeContextMenu(&ContextNodeId))
	{
		ImGui::OpenPopup("LuaBlueprintNodeMenu");
	}
	else if (ed::ShowPinContextMenu(&ContextPinId))
	{
		ImGui::OpenPopup("LuaBlueprintPinMenu");
	}
	else if (ed::ShowLinkContextMenu(&ContextLinkId))
	{
		ImGui::OpenPopup("LuaBlueprintLinkMenu");
	}
	else if (ed::ShowBackgroundContextMenu())
	{
		PendingNewNodePosition = ed::ScreenToCanvas(ImGui::GetMousePos());
		AddNodeSearchBuf[0] = 0;
		ImGui::OpenPopup("LuaBlueprintBackgroundMenu");
	}

	if (ImGui::BeginPopup("LuaBlueprintNodeMenu"))
	{
		if (ImGui::MenuItem("Delete"))
		{
			if (Blueprint->RemoveNode(NodeIdToU32(ContextNodeId))) MarkDirty();
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("LuaBlueprintLinkMenu"))
	{
		if (ImGui::MenuItem("Break Link"))
		{
			if (Blueprint->RemoveLink(LinkIdToU32(ContextLinkId))) MarkDirty();
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("LuaBlueprintPinMenu"))
	{
		ImGui::TextDisabled("Pin #%u", PinIdToU32(ContextPinId));
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("LuaBlueprintBackgroundMenu"))
	{
		RenderAddNodeMenu(Blueprint);
		ImGui::EndPopup();
	}

	// 변수 drop → Get/Set 팝업 (캔버스에서 직접 받는 drop 은 ed context 에 막혀
	// background context menu 와 같은 timing 으로 처리).
	if (bShowVariableDropMenu)
	{
		ImGui::OpenPopup("LuaBlueprintVariableDropMenu");
		bShowVariableDropMenu = false;
	}
	if (ImGui::BeginPopup("LuaBlueprintVariableDropMenu"))
	{
		ImGui::TextDisabled("%s", PendingVariableDropName.ToString().c_str());
		ImGui::Separator();
		if (ImGui::MenuItem("Get"))
		{
			SpawnVariableNode(Blueprint, ELuaBlueprintNodeType::GetVariable, PendingVariableDropName, PendingVariableDropPos);
		}
		if (ImGui::MenuItem("Set"))
		{
			SpawnVariableNode(Blueprint, ELuaBlueprintNodeType::SetVariable, PendingVariableDropName, PendingVariableDropPos);
		}
		ImGui::EndPopup();
	}
	ed::Resume();

	{
		ed::NodeId SelectedNodes[1];
		const int SelectedCount = ed::GetSelectedNodes(SelectedNodes, 1);
		if (SelectedCount > 0)
		{
			SelectedNodeId = NodeIdToU32(SelectedNodes[0]);
		}
	}

	ed::End();
	ed::SetCurrentEditor(nullptr);

	// 캔버스 child 위의 빈 영역에서 drag-drop 수신. ed 컨텍스트는 자체 hit-test 를 하지만,
	// EndChild 직전 dummy invisible button 으로 받는 게 가장 안정적.
	HandleVariableDropOnCanvas();

	ImGui::EndChild();

	if (CanvasWidth < TotalWidth)
	{
		ImGui::SameLine();
		ImGui::BeginChild("##LuaBlueprintInspector", ImVec2(0, 0), ImGuiChildFlags_Borders);
		if (SelectedNodeId != 0)
		{
			if (FLuaBlueprintNode* Node = Blueprint->FindNode(SelectedNodeId))
			{
				RenderNodeInspector(Blueprint, *Node);
			}
			else
			{
				ImGui::TextDisabled("Stale selection.");
			}
		}
		else
		{
			ImGui::TextDisabled("Select a node to edit details.");
		}
		ImGui::EndChild();
	}
}

void FLuaBlueprintEditorWidget::HandleVariableDropOnCanvas()
{
	// 캔버스 child window 의 전체 영역을 custom drop target 으로 사용.
	// ScreenToCanvas 변환은 다음 프레임의 ed::Begin 안에서 안전하게 처리 (bPendingVariableDrop 플래그).
	const ImGuiID ChildId = ImGui::GetCurrentWindow()->ID;
	const ImRect ChildRect(ImGui::GetWindowPos(),
		ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x,
		       ImGui::GetWindowPos().y + ImGui::GetWindowSize().y));
	if (ImGui::BeginDragDropTargetCustom(ChildRect, ChildId))
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("LuaBlueprintVariable"))
		{
			if (Payload->Data && Payload->DataSize == static_cast<int>(sizeof(FName)))
			{
				PendingVariableDropName  = *reinterpret_cast<const FName*>(Payload->Data);
				PendingVariableScreenPos = ImGui::GetMousePos();
				bPendingVariableDrop     = true;
			}
		}
		ImGui::EndDragDropTarget();
	}
}

void FLuaBlueprintEditorWidget::RenderNodeInspector(ULuaBlueprintAsset* Blueprint, FLuaBlueprintNode& Node)
{
	ImGui::TextColored(NodeHeaderColor(Node.Type), "%s", NodeTypeLabel(Node.Type));
	ImGui::TextDisabled("Node #%u", Node.NodeId);
	ImGui::Separator();

	char DisplayBuf[128];
	CopyToBuffer(DisplayBuf, sizeof(DisplayBuf), Node.DisplayName.ToString());
	if (ImGui::InputText("Display", DisplayBuf, sizeof(DisplayBuf), ImGuiInputTextFlags_EnterReturnsTrue))
	{
		Node.DisplayName = DisplayBuf[0] ? FName(DisplayBuf) : FName(NodeTypeLabel(Node.Type));
		Blueprint->BumpVersion();
		MarkDirty();
	}

	switch (Node.Type)
	{
	case ELuaBlueprintNodeType::GetVariable:
	case ELuaBlueprintNodeType::SetVariable:
	{
		FString CurrentName = Node.NameValue.ToString();
		const char* Preview = CurrentName.empty() ? "(none)" : CurrentName.c_str();
		if (ImGui::BeginCombo("Variable", Preview))
		{
			for (const FLuaBlueprintVariable& Variable : Blueprint->GetVariables())
			{
				const FString VarName = Variable.Name.ToString();
				const bool bSelected = (VarName == CurrentName);
				if (ImGui::Selectable(VarName.c_str(), bSelected))
				{
					Node.NameValue = Variable.Name;
					Blueprint->BumpVersion();
					MarkDirty();
				}
				if (bSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		char NameBuf[128];
		CopyToBuffer(NameBuf, sizeof(NameBuf), Node.NameValue.ToString());
		if (ImGui::InputText("Variable Name", NameBuf, sizeof(NameBuf), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			Node.NameValue = NameBuf[0] ? FName(NameBuf) : FName::None;
			Blueprint->BumpVersion();
			MarkDirty();
		}
		break;
	}
	case ELuaBlueprintNodeType::GetProperty:
	case ELuaBlueprintNodeType::SetProperty:
	case ELuaBlueprintNodeType::CallFunction:
	{
		char NameBuf[160];
		CopyToBuffer(NameBuf, sizeof(NameBuf), Node.NameValue.ToString());
		if (ImGui::InputText(Node.Type == ELuaBlueprintNodeType::CallFunction ? "Function" : "Property", NameBuf, sizeof(NameBuf), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			Node.NameValue = NameBuf[0] ? FName(NameBuf) : FName::None;
			Blueprint->BumpVersion();
			MarkDirty();
		}
		break;
	}
	case ELuaBlueprintNodeType::CallFunctionSignature:
	{
		char SigBuf[256];
		CopyToBuffer(SigBuf, sizeof(SigBuf), Node.StringValue.empty() ? Node.NameValue.ToString() : Node.StringValue);
		if (ImGui::InputText("Signature", SigBuf, sizeof(SigBuf), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			Node.StringValue = SigBuf;
			Blueprint->BumpVersion();
			MarkDirty();
		}
		break;
	}
	case ELuaBlueprintNodeType::PrintString:
	case ELuaBlueprintNodeType::LiteralString:
	{
		char TextBuf[512];
		CopyToBuffer(TextBuf, sizeof(TextBuf), Node.StringValue);
		if (ImGui::InputTextMultiline("Text", TextBuf, sizeof(TextBuf), ImVec2(-1, 90)))
		{
			Node.StringValue = TextBuf;
			// PrintString 의 경우 Text 입력 핀의 DefaultString 도 동기화 — 핀 inline editor 와의 일관성.
			if (Node.Type == ELuaBlueprintNodeType::PrintString)
			{
				for (FLuaBlueprintPin& Pin : Node.Pins)
				{
					if (Pin.Kind == ELuaBlueprintPinKind::Input
						&& Pin.Type == ELuaBlueprintPinType::String
						&& Pin.DisplayName.ToString() == "Text")
					{
						Pin.DefaultString = Node.StringValue;
						break;
					}
				}
			}
			Blueprint->BumpVersion();
			MarkDirty();
		}
		break;
	}
	case ELuaBlueprintNodeType::LiteralBool:
		if (ImGui::Checkbox("Value", &Node.BoolValue)) { Blueprint->BumpVersion(); MarkDirty(); }
		break;
	case ELuaBlueprintNodeType::LiteralInt:
		if (ImGui::InputInt("Value", &Node.IntValue)) { Blueprint->BumpVersion(); MarkDirty(); }
		break;
	case ELuaBlueprintNodeType::LiteralFloat:
		if (ImGui::InputFloat("Value", &Node.FloatValue)) { Blueprint->BumpVersion(); MarkDirty(); }
		break;
	case ELuaBlueprintNodeType::LiteralVector:
	{
		float V[3] = { Node.VectorValue.X, Node.VectorValue.Y, Node.VectorValue.Z };
		if (ImGui::InputFloat3("Value", V))
		{
			Node.VectorValue = FVector(V[0], V[1], V[2]);
			Blueprint->BumpVersion();
			MarkDirty();
		}
		break;
	}
	// 클래스 이름이 컴파일 타임에 묶이는 노드들 — 드롭다운 + 직접 입력.
	case ELuaBlueprintNodeType::SpawnActor:
	case ELuaBlueprintNodeType::FindActorByClass:
	case ELuaBlueprintNodeType::FindActorsByClass:
	case ELuaBlueprintNodeType::Cast:
	case ELuaBlueprintNodeType::ForEachActorByClass:
	{
		FString Current = Node.NameValue.ToString();
		const char* Preview = Current.empty() ? "(none)" : Current.c_str();
		if (ImGui::BeginCombo("Class", Preview))
		{
			for (UClass* C : UClass::GetAllClasses())
			{
				if (!C) continue;
				const bool bSelected = (Current == C->GetName());
				if (ImGui::Selectable(C->GetName(), bSelected))
				{
					Node.NameValue = FName(C->GetName());
					Blueprint->BumpVersion();
					MarkDirty();
				}
				if (bSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		char Buf[160];
		CopyToBuffer(Buf, sizeof(Buf), Node.NameValue.ToString());
		if (ImGui::InputText("Class Name", Buf, sizeof(Buf), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			Node.NameValue = Buf[0] ? FName(Buf) : FName::None;
			Blueprint->BumpVersion();
			MarkDirty();
		}
		break;
	}
	// Tag 가 컴파일 타임에 묶이는 노드 (ForEachActorByTag). FindActorByTag 류는 input pin 으로도 가능.
	case ELuaBlueprintNodeType::ForEachActorByTag:
	{
		char Buf[128];
		CopyToBuffer(Buf, sizeof(Buf), Node.StringValue);
		if (ImGui::InputText("Tag", Buf, sizeof(Buf), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			Node.StringValue = Buf;
			Blueprint->BumpVersion();
			MarkDirty();
		}
		break;
	}
	default:
		break;
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Pins");
	for (const FLuaBlueprintPin& Pin : Node.Pins)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, PinTypeColor(Pin.Type));
		ImGui::BulletText("%s %s %s #%u",
			Pin.Kind == ELuaBlueprintPinKind::Input ? "In" : "Out",
			PinTypeLabel(Pin.Type),
			Pin.DisplayName.ToString().c_str(),
			Pin.PinId);
		ImGui::PopStyleColor();
	}
}

void FLuaBlueprintEditorWidget::RenderDiagnostics(ULuaBlueprintAsset* Blueprint)
{
	if (Blueprint->GetDiagnostics().empty())
	{
		ImGui::TextDisabled("No diagnostics.");
		return;
	}

	for (const FLuaBlueprintDiagnostic& Diagnostic : Blueprint->GetDiagnostics())
	{
		ImVec4 Color(0.8f, 0.8f, 0.8f, 1.0f);
		switch (Diagnostic.Severity)
		{
		case ELuaBlueprintDiagnosticSeverity::Error:   Color = ImVec4(1.0f, 0.45f, 0.45f, 1.0f); break;
		case ELuaBlueprintDiagnosticSeverity::Warning: Color = ImVec4(0.95f, 0.85f, 0.35f, 1.0f); break;
		default: break;
		}
		ImGui::TextColored(Color, "[%s] Node %u: %s", SeverityLabel(Diagnostic.Severity), Diagnostic.NodeId, Diagnostic.Message.c_str());
	}
}

void FLuaBlueprintEditorWidget::RenderGeneratedLua(ULuaBlueprintAsset* Blueprint)
{
	const FString& Source = Blueprint->GetGeneratedLuaSource();
	if (Source.empty())
	{
		ImGui::TextDisabled("No generated source. Compile the blueprint first.");
		return;
	}

	ImGui::InputTextMultiline(
		"##GeneratedLua",
		const_cast<char*>(Source.c_str()),
		Source.size() + 1,
		ImVec2(-1.0f, -1.0f),
		ImGuiInputTextFlags_ReadOnly
	);
}
