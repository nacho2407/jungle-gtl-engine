#include "Editor/UI/Asset/LuaBlueprint/LuaBlueprintEditorWidget.h"

#include "LuaBlueprint/LuaBlueprintAsset.h"
#include "LuaBlueprint/LuaBlueprintManager.h"
#include "Object/Object.h"

#include "imgui.h"
#include "imgui_internal.h" 
#include "imgui_node_editor.h"

#include "Object/Reflection/UClass.h"
#include "Serialization/MemoryArchive.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace ed = ax::NodeEditor;

namespace
{
    inline ed::NodeId ToNodeId(uint32 Id) { return static_cast<ed::NodeId>(Id); }
    inline ed::PinId  ToPinId(uint32 Id) { return static_cast<ed::PinId>(Id); }
    inline ed::LinkId ToLinkId(uint32 Id) { return static_cast<ed::LinkId>(Id); }

    inline uint32 NodeIdToU32(ed::NodeId Id) { return static_cast<uint32>(Id.Get()); }
    inline uint32 PinIdToU32(ed::PinId Id) { return static_cast<uint32>(Id.Get()); }
    inline uint32 LinkIdToU32(ed::LinkId Id) { return static_cast<uint32>(Id.Get()); }

    struct FScopedNodeEditorCurrent
    {
        ed::EditorContext* Previous = nullptr;
        ed::EditorContext* Desired  = nullptr;

        explicit FScopedNodeEditorCurrent(ed::EditorContext* InDesired)
            : Desired(InDesired)
        {
            Previous = ed::GetCurrentEditor();
            if (Desired && Previous != Desired)
            {
                ed::SetCurrentEditor(Desired);
            }
        }

        ~FScopedNodeEditorCurrent()
        {
            if (Desired && Previous != Desired)
            {
                ed::SetCurrentEditor(Previous);
            }
        }
    };

    void CopyToBuffer(char* Buffer, size_t BufferSize, const FString& Value)
    {
        if (!Buffer || BufferSize == 0) return;
        std::snprintf(Buffer, BufferSize, "%s", Value.c_str());
    }

    const char* NodeTypeLabel(ELuaBlueprintNodeType Type)
    {
        switch (Type)
        {
        case ELuaBlueprintNodeType::EventBeginPlay:
            return "Event BeginPlay";
        case ELuaBlueprintNodeType::EventTick:
            return "Event Tick";
        case ELuaBlueprintNodeType::EventEndPlay:
            return "Event EndPlay";
        case ELuaBlueprintNodeType::EventOverlap:
            return "Event Overlap";
        case ELuaBlueprintNodeType::EventEndOverlap:
            return "Event EndOverlap";
        case ELuaBlueprintNodeType::EventHit:
            return "Event Hit";
        case ELuaBlueprintNodeType::EventEndHit:
            return "Event EndHit";
        case ELuaBlueprintNodeType::Sequence:
            return "Sequence";
        case ELuaBlueprintNodeType::Branch:
            return "Branch";
        case ELuaBlueprintNodeType::ForLoop:
            return "For Loop";
        case ELuaBlueprintNodeType::WhileLoop:
            return "While Loop";
        case ELuaBlueprintNodeType::PrintString:
            return "Print String";
        case ELuaBlueprintNodeType::LiteralBool:
            return "Literal Bool";
        case ELuaBlueprintNodeType::LiteralInt:
            return "Literal Int";
        case ELuaBlueprintNodeType::LiteralFloat:
            return "Literal Float";
        case ELuaBlueprintNodeType::LiteralString:
            return "Literal String";
        case ELuaBlueprintNodeType::LiteralVector:
            return "Literal Vector";
        case ELuaBlueprintNodeType::GetVariable:
            return "Get Variable";
        case ELuaBlueprintNodeType::SetVariable:
            return "Set Variable";
        case ELuaBlueprintNodeType::GetProperty:
            return "Get Property";
        case ELuaBlueprintNodeType::SetProperty:
            return "Set Property";
        case ELuaBlueprintNodeType::CallFunction:
            return "Call Function";
        case ELuaBlueprintNodeType::CallFunctionSignature:
            return "Call Signature";
        case ELuaBlueprintNodeType::Self:
            return "Self (Owning Actor)";
        case ELuaBlueprintNodeType::AddFloat:
            return "Float + Float";
        case ELuaBlueprintNodeType::SubtractFloat:
            return "Float - Float";
        case ELuaBlueprintNodeType::MultiplyFloat:
            return "Float * Float";
        case ELuaBlueprintNodeType::DivideFloat:
            return "Float / Float";
        case ELuaBlueprintNodeType::AddInt:
            return "Int + Int";
        case ELuaBlueprintNodeType::SubtractInt:
            return "Int - Int";
        case ELuaBlueprintNodeType::MultiplyInt:
            return "Int * Int";
        case ELuaBlueprintNodeType::DivideInt:
            return "Int / Int";
        case ELuaBlueprintNodeType::ModInt:
            return "Int % Int";
        case ELuaBlueprintNodeType::EqualFloat:
            return "Float == Float";
        case ELuaBlueprintNodeType::NotEqualFloat:
            return "Float != Float";
        case ELuaBlueprintNodeType::LessFloat:
            return "Float < Float";
        case ELuaBlueprintNodeType::GreaterFloat:
            return "Float > Float";
        case ELuaBlueprintNodeType::LessEqualFloat:
            return "Float <= Float";
        case ELuaBlueprintNodeType::GreaterEqualFloat:
            return "Float >= Float";
        case ELuaBlueprintNodeType::EqualInt:
            return "Int == Int";
        case ELuaBlueprintNodeType::NotEqualInt:
            return "Int != Int";
        case ELuaBlueprintNodeType::LessInt:
            return "Int < Int";
        case ELuaBlueprintNodeType::GreaterInt:
            return "Int > Int";
        case ELuaBlueprintNodeType::And:
            return "Bool AND Bool";
        case ELuaBlueprintNodeType::Or:
            return "Bool OR Bool";
        case ELuaBlueprintNodeType::Not:
            return "NOT Bool";
        case ELuaBlueprintNodeType::AppendString:
            return "Append String";
        case ELuaBlueprintNodeType::MakeVector:
            return "Make Vector";
        case ELuaBlueprintNodeType::BreakVector:
            return "Break Vector";
        case ELuaBlueprintNodeType::AddVector:
            return "Vector + Vector";
        case ELuaBlueprintNodeType::SubtractVector:
            return "Vector - Vector";
        case ELuaBlueprintNodeType::ScaleVector:
            return "Vector * Float";
        case ELuaBlueprintNodeType::DotVector:
            return "Dot";
        case ELuaBlueprintNodeType::CrossVector:
            return "Cross";
        case ELuaBlueprintNodeType::VectorLength:
            return "Vector Length";
        case ELuaBlueprintNodeType::NormalizeVector:
            return "Normalize";
        case ELuaBlueprintNodeType::SpawnActor:
            return "Spawn Actor";
        case ELuaBlueprintNodeType::DestroyActor:
            return "Destroy Actor";
        case ELuaBlueprintNodeType::FindActorByName:
            return "Find Actor by Name";
        case ELuaBlueprintNodeType::FindActorByClass:
            return "Find Actor of Class";
        case ELuaBlueprintNodeType::FindActorByTag:
            return "Find Actor with Tag";
        case ELuaBlueprintNodeType::FindActorsByTag:
            return "Find Actors with Tag";
        case ELuaBlueprintNodeType::FindActorsByClass:
            return "Find Actors of Class";
        case ELuaBlueprintNodeType::GetActorLocation:
            return "Get Actor Location";
        case ELuaBlueprintNodeType::SetActorLocation:
            return "Set Actor Location";
        case ELuaBlueprintNodeType::GetActorRotation:
            return "Get Actor Rotation";
        case ELuaBlueprintNodeType::SetActorRotation:
            return "Set Actor Rotation";
        case ELuaBlueprintNodeType::GetActorScale:
            return "Get Actor Scale";
        case ELuaBlueprintNodeType::SetActorScale:
            return "Set Actor Scale";
        case ELuaBlueprintNodeType::GetActorForward:
            return "Get Actor Forward";
        case ELuaBlueprintNodeType::GetActorRight:
            return "Get Actor Right";
        case ELuaBlueprintNodeType::AddActorWorldOffset:
            return "Add Actor World Offset";
        case ELuaBlueprintNodeType::ActorHasTag:
            return "Actor Has Tag";
        case ELuaBlueprintNodeType::ActorAddTag:
            return "Actor Add Tag";
        case ELuaBlueprintNodeType::ActorRemoveTag:
            return "Actor Remove Tag";
        case ELuaBlueprintNodeType::GetActorName:
            return "Get Actor Name";
        case ELuaBlueprintNodeType::GetOwnerActor:
            return "Get Owner Actor";
        case ELuaBlueprintNodeType::IsValid:
            return "Is Valid";
        case ELuaBlueprintNodeType::Cast:
            return "Cast";
        case ELuaBlueprintNodeType::GetRootComponent:
            return "Get Root Component";
        case ELuaBlueprintNodeType::GetComponentByName:
            return "Get Component by Name";
        case ELuaBlueprintNodeType::GetPrimitiveComponent:
            return "Get Primitive Component";
        case ELuaBlueprintNodeType::ActivateComponent:
            return "Activate";
        case ELuaBlueprintNodeType::DeactivateComponent:
            return "Deactivate";
        case ELuaBlueprintNodeType::AddForce:
            return "Add Force";
        case ELuaBlueprintNodeType::AddTorque:
            return "Add Torque";
        case ELuaBlueprintNodeType::GetLinearVelocity:
            return "Get Linear Velocity";
        case ELuaBlueprintNodeType::SetLinearVelocity:
            return "Set Linear Velocity";
        case ELuaBlueprintNodeType::GetMass:
            return "Get Mass";
        case ELuaBlueprintNodeType::SetSimulatePhysics:
            return "Set Simulate Physics";
        case ELuaBlueprintNodeType::Lerp:
            return "Lerp";
        case ELuaBlueprintNodeType::Clamp:
            return "Clamp";
        case ELuaBlueprintNodeType::Min:
            return "Min";
        case ELuaBlueprintNodeType::Max:
            return "Max";
        case ELuaBlueprintNodeType::RandomFloat:
            return "Random Float";
        case ELuaBlueprintNodeType::RandomInt:
            return "Random Int";
        case ELuaBlueprintNodeType::Sin:
            return "Sin";
        case ELuaBlueprintNodeType::Cos:
            return "Cos";
        case ELuaBlueprintNodeType::Sqrt:
            return "Sqrt";
        case ELuaBlueprintNodeType::AbsFloat:
            return "Abs";
        case ELuaBlueprintNodeType::Floor:
            return "Floor";
        case ELuaBlueprintNodeType::Ceil:
            return "Ceil";
        case ELuaBlueprintNodeType::Distance:
            return "Distance";
        case ELuaBlueprintNodeType::GetGameTime:
            return "Get Game Time";
        case ELuaBlueprintNodeType::ForEachActorByClass:
            return "For Each Actor (Class)";
        case ELuaBlueprintNodeType::ForEachActorByTag:
            return "For Each Actor (Tag)";
        case ELuaBlueprintNodeType::ForEachArray:
            return "For Each Array";
        case ELuaBlueprintNodeType::Reroute:
            return "Reroute";
        case ELuaBlueprintNodeType::Comment:
            return "Comment";
        case ELuaBlueprintNodeType::CustomEvent:
            return "Custom Event";
        case ELuaBlueprintNodeType::CallCustomEvent:
            return "Call Custom Event";
        case ELuaBlueprintNodeType::Delay:
            return "Delay";
        case ELuaBlueprintNodeType::ToBool:
            return "To Bool";
        case ELuaBlueprintNodeType::ToInt:
            return "To Int";
        case ELuaBlueprintNodeType::ToFloat:
            return "To Float";
        case ELuaBlueprintNodeType::ToString:
            return "To String";
        case ELuaBlueprintNodeType::ToVector:
            return "To Vector";
        case ELuaBlueprintNodeType::BindEvent:
            return "Bind Event";
        case ELuaBlueprintNodeType::UnbindEvent:
            return "Unbind Event";
        case ELuaBlueprintNodeType::HasEventBinding:
            return "Has Event Binding";
        }
        return "Node";
    }


    const char* NodeTypeHelpText(ELuaBlueprintNodeType Type)
    {
        switch (Type)
        {
        case ELuaBlueprintNodeType::EventBeginPlay:       return "Exec entry fired when the owning actor begins play.";
        case ELuaBlueprintNodeType::EventTick:            return "Exec entry fired every frame. DeltaSeconds carries frame time.";
        case ELuaBlueprintNodeType::EventEndPlay:         return "Exec entry fired when the owning actor ends play.";
        case ELuaBlueprintNodeType::EventOverlap:         return "Exec entry fired on overlap begin, with actor/component payloads.";
        case ELuaBlueprintNodeType::EventEndOverlap:      return "Exec entry fired on overlap end.";
        case ELuaBlueprintNodeType::EventHit:             return "Exec entry fired when a blocking hit occurs.";
        case ELuaBlueprintNodeType::EventEndHit:          return "Exec entry fired for the matching hit-end event if available.";
        case ELuaBlueprintNodeType::Sequence:             return "Runs multiple exec outputs in order.";
        case ELuaBlueprintNodeType::Branch:               return "Chooses True or False exec flow from a Bool condition.";
        case ELuaBlueprintNodeType::ForLoop:              return "Loops from FirstIndex to LastIndex and then fires Completed.";
        case ELuaBlueprintNodeType::WhileLoop:            return "Repeats while Condition remains true. Keep the condition safe.";
        case ELuaBlueprintNodeType::PrintString:          return "Prints text for debugging or quick feedback.";
        case ELuaBlueprintNodeType::LiteralBool:          return "Constant Bool value.";
        case ELuaBlueprintNodeType::LiteralInt:           return "Constant Int value.";
        case ELuaBlueprintNodeType::LiteralFloat:         return "Constant Float value.";
        case ELuaBlueprintNodeType::LiteralString:        return "Constant String value.";
        case ELuaBlueprintNodeType::LiteralVector:        return "Constant Vector value.";
        case ELuaBlueprintNodeType::GetVariable:          return "Reads a graph variable.";
        case ELuaBlueprintNodeType::SetVariable:          return "Writes a graph variable and continues exec flow.";
        case ELuaBlueprintNodeType::GetProperty:          return "Reads a named reflected property from the target object.";
        case ELuaBlueprintNodeType::SetProperty:          return "Writes a named reflected property on the target object.";
        case ELuaBlueprintNodeType::CallFunction:         return "Calls a reflected function by name.";
        case ELuaBlueprintNodeType::CallFunctionSignature:return "Calls a function using an explicit signature string.";
        case ELuaBlueprintNodeType::Self:                 return "Returns the owning actor/object for this Lua Blueprint.";
        case ELuaBlueprintNodeType::AddFloat:             return "Float addition.";
        case ELuaBlueprintNodeType::SubtractFloat:        return "Float subtraction.";
        case ELuaBlueprintNodeType::MultiplyFloat:        return "Float multiplication.";
        case ELuaBlueprintNodeType::DivideFloat:          return "Float division.";
        case ELuaBlueprintNodeType::AddInt:               return "Integer addition.";
        case ELuaBlueprintNodeType::SubtractInt:          return "Integer subtraction.";
        case ELuaBlueprintNodeType::MultiplyInt:          return "Integer multiplication.";
        case ELuaBlueprintNodeType::DivideInt:            return "Integer division.";
        case ELuaBlueprintNodeType::ModInt:               return "Integer remainder.";
        case ELuaBlueprintNodeType::EqualFloat:           return "Float equality comparison.";
        case ELuaBlueprintNodeType::NotEqualFloat:        return "Float inequality comparison.";
        case ELuaBlueprintNodeType::LessFloat:            return "Float less-than comparison.";
        case ELuaBlueprintNodeType::GreaterFloat:         return "Float greater-than comparison.";
        case ELuaBlueprintNodeType::LessEqualFloat:       return "Float less-or-equal comparison.";
        case ELuaBlueprintNodeType::GreaterEqualFloat:    return "Float greater-or-equal comparison.";
        case ELuaBlueprintNodeType::EqualInt:             return "Integer equality comparison.";
        case ELuaBlueprintNodeType::NotEqualInt:          return "Integer inequality comparison.";
        case ELuaBlueprintNodeType::LessInt:              return "Integer less-than comparison.";
        case ELuaBlueprintNodeType::GreaterInt:           return "Integer greater-than comparison.";
        case ELuaBlueprintNodeType::And:                  return "Boolean AND.";
        case ELuaBlueprintNodeType::Or:                   return "Boolean OR.";
        case ELuaBlueprintNodeType::Not:                  return "Boolean NOT.";
        case ELuaBlueprintNodeType::AppendString:         return "Concatenates strings.";
        case ELuaBlueprintNodeType::MakeVector:           return "Builds a Vector from X/Y/Z components.";
        case ELuaBlueprintNodeType::BreakVector:          return "Splits a Vector into X/Y/Z components.";
        case ELuaBlueprintNodeType::AddVector:            return "Vector addition.";
        case ELuaBlueprintNodeType::SubtractVector:       return "Vector subtraction.";
        case ELuaBlueprintNodeType::ScaleVector:          return "Scales a Vector by a Float.";
        case ELuaBlueprintNodeType::DotVector:            return "Dot product of two Vectors.";
        case ELuaBlueprintNodeType::CrossVector:          return "Cross product of two Vectors.";
        case ELuaBlueprintNodeType::VectorLength:         return "Returns Vector length.";
        case ELuaBlueprintNodeType::NormalizeVector:      return "Returns a normalized Vector direction.";
        case ELuaBlueprintNodeType::SpawnActor:           return "Spawns an actor of the configured class.";
        case ELuaBlueprintNodeType::DestroyActor:         return "Destroys the target actor.";
        case ELuaBlueprintNodeType::FindActorByName:      return "Finds one actor by name.";
        case ELuaBlueprintNodeType::FindActorByClass:     return "Finds one actor by class.";
        case ELuaBlueprintNodeType::FindActorByTag:       return "Finds one actor with the tag.";
        case ELuaBlueprintNodeType::FindActorsByTag:      return "Finds all actors with the tag.";
        case ELuaBlueprintNodeType::FindActorsByClass:    return "Finds all actors of the class.";
        case ELuaBlueprintNodeType::GetActorLocation:     return "Reads target actor location.";
        case ELuaBlueprintNodeType::SetActorLocation:     return "Sets target actor location.";
        case ELuaBlueprintNodeType::GetActorRotation:     return "Reads target actor rotation.";
        case ELuaBlueprintNodeType::SetActorRotation:     return "Sets target actor rotation.";
        case ELuaBlueprintNodeType::GetActorScale:        return "Reads target actor scale.";
        case ELuaBlueprintNodeType::SetActorScale:        return "Sets target actor scale.";
        case ELuaBlueprintNodeType::GetActorForward:      return "Returns target actor forward vector.";
        case ELuaBlueprintNodeType::GetActorRight:        return "Returns target actor right vector.";
        case ELuaBlueprintNodeType::AddActorWorldOffset:  return "Moves an actor by a world-space offset.";
        case ELuaBlueprintNodeType::ActorHasTag:          return "Checks whether an actor owns a tag.";
        case ELuaBlueprintNodeType::ActorAddTag:          return "Adds a tag to an actor.";
        case ELuaBlueprintNodeType::ActorRemoveTag:       return "Removes a tag from an actor.";
        case ELuaBlueprintNodeType::GetActorName:         return "Returns the actor name.";
        case ELuaBlueprintNodeType::GetOwnerActor:        return "Returns the owner actor when available.";
        case ELuaBlueprintNodeType::IsValid:              return "Checks whether an object reference is valid.";
        case ELuaBlueprintNodeType::Cast:                 return "Casts an object reference to the configured class.";
        case ELuaBlueprintNodeType::GetRootComponent:     return "Gets an actor root component.";
        case ELuaBlueprintNodeType::GetComponentByName:   return "Gets a component by name.";
        case ELuaBlueprintNodeType::GetPrimitiveComponent:return "Gets a primitive component from the actor/component target.";
        case ELuaBlueprintNodeType::ActivateComponent:    return "Activates the target component.";
        case ELuaBlueprintNodeType::DeactivateComponent:  return "Deactivates the target component.";
        case ELuaBlueprintNodeType::AddForce:             return "Applies force to a primitive component.";
        case ELuaBlueprintNodeType::AddTorque:            return "Applies torque to a primitive component.";
        case ELuaBlueprintNodeType::GetLinearVelocity:    return "Reads linear velocity from a primitive component.";
        case ELuaBlueprintNodeType::SetLinearVelocity:    return "Sets linear velocity on a primitive component.";
        case ELuaBlueprintNodeType::GetMass:              return "Reads mass from a primitive component.";
        case ELuaBlueprintNodeType::SetSimulatePhysics:   return "Enables or disables physics simulation.";
        case ELuaBlueprintNodeType::Lerp:                 return "Linearly interpolates between two values.";
        case ELuaBlueprintNodeType::Clamp:                return "Clamps a value between Min and Max.";
        case ELuaBlueprintNodeType::Min:                  return "Returns the smaller value.";
        case ELuaBlueprintNodeType::Max:                  return "Returns the larger value.";
        case ELuaBlueprintNodeType::RandomFloat:          return "Returns a random Float in range.";
        case ELuaBlueprintNodeType::RandomInt:            return "Returns a random Int in range.";
        case ELuaBlueprintNodeType::Sin:                  return "Sine of the input angle/value.";
        case ELuaBlueprintNodeType::Cos:                  return "Cosine of the input angle/value.";
        case ELuaBlueprintNodeType::Sqrt:                 return "Square root.";
        case ELuaBlueprintNodeType::AbsFloat:             return "Absolute Float value.";
        case ELuaBlueprintNodeType::Floor:                return "Rounds down.";
        case ELuaBlueprintNodeType::Ceil:                 return "Rounds up.";
        case ELuaBlueprintNodeType::Distance:             return "Distance between two vectors.";
        case ELuaBlueprintNodeType::GetGameTime:          return "Returns current game time.";
        case ELuaBlueprintNodeType::ForEachActorByClass:  return "Iterates actors of a class.";
        case ELuaBlueprintNodeType::ForEachActorByTag:    return "Iterates actors with a tag.";
        case ELuaBlueprintNodeType::ForEachArray:         return "Iterates elements in an array.";
        case ELuaBlueprintNodeType::Reroute:              return "Visual pass-through node for cleaner wiring.";
        case ELuaBlueprintNodeType::Comment:              return "Resizable group/comment box. Deleting it also deletes contained nodes.";
        case ELuaBlueprintNodeType::CustomEvent:          return "Defines a custom exec entry that can be called or bound.";
        case ELuaBlueprintNodeType::CallCustomEvent:      return "Calls a Custom Event by name.";
        case ELuaBlueprintNodeType::Delay:                return "Waits for a duration before continuing exec flow.";
        case ELuaBlueprintNodeType::ToBool:               return "Converts a value to Bool.";
        case ELuaBlueprintNodeType::ToInt:                return "Converts a value to Int.";
        case ELuaBlueprintNodeType::ToFloat:              return "Converts a value to Float.";
        case ELuaBlueprintNodeType::ToString:             return "Converts a value to String.";
        case ELuaBlueprintNodeType::ToVector:             return "Converts a value to Vector.";
        case ELuaBlueprintNodeType::BindEvent:            return "Binds a reflected event to a Custom Event callback.";
        case ELuaBlueprintNodeType::UnbindEvent:          return "Removes a reflected event binding.";
        case ELuaBlueprintNodeType::HasEventBinding:      return "Checks whether a reflected event binding exists.";
        }
        return "Lua Blueprint node.";
    }

    bool RenderNodeHelpIcon(ELuaBlueprintNodeType Type, ELuaBlueprintNodeType& OutHoveredType)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (!ImGui::IsItemHovered())
        {
            return false;
        }

        // Do not open an ImGui tooltip while ax::NodeEditor is actively drawing a node.
        // The node editor applies its own canvas transform/clipping while nodes are emitted,
        // which can make regular BeginTooltip/SetTooltip appear at a seemingly unrelated
        // screen position. Capture only the hovered help target here and render one overlay
        // after ed::End(), constrained to the owning Lua Blueprint window/canvas.
        OutHoveredType = Type;
        return true;
    }

    void RenderNodeHelpTooltip(ELuaBlueprintNodeType Type, const ImRect& OwnerScreenRect, ImGuiID OwnerViewportId)
    {
        const ImGuiStyle& Style = ImGui::GetStyle();
        constexpr float Margin = 8.0f;

        // Keep sizing relative to the Blueprint owner rect, not the application's main viewport.
        // This prevents the tooltip from snapping to the global top-left when the Blueprint
        // editor is floating, docked away from (0,0), or rendered on a secondary viewport.
        const float OwnerWidth = std::max(1.0f, OwnerScreenRect.Max.x - OwnerScreenRect.Min.x);
        const float PreferredWindowWidth = ImGui::GetFontSize() * 34.0f + Style.WindowPadding.x * 2.0f;
        const float AvailableWindowWidth = std::max(96.0f, OwnerWidth - Margin * 2.0f);
        const float TooltipWindowWidth = std::min(PreferredWindowWidth, AvailableWindowWidth);
        const float TextWrapWidth = std::max(64.0f, TooltipWindowWidth - Style.WindowPadding.x * 2.0f);

        ImVec2 Pos(OwnerScreenRect.Max.x - Margin, OwnerScreenRect.Min.y + Margin);
        Pos.x = std::max(OwnerScreenRect.Min.x + Margin, Pos.x);
        Pos.y = std::max(OwnerScreenRect.Min.y + Margin, Pos.y);

        if (OwnerViewportId != 0)
        {
            ImGui::SetNextWindowViewport(OwnerViewportId);
        }
        ImGui::SetNextWindowPos(Pos, ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.96f);
        ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(TooltipWindowWidth, FLT_MAX));

        constexpr ImGuiWindowFlags TooltipFlags =
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoInputs;

        if (ImGui::Begin("##LuaBPNodeHelpTooltip", nullptr, TooltipFlags))
        {
            ImGui::TextUnformatted(NodeTypeLabel(Type));
            ImGui::Separator();
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + TextWrapWidth);
            ImGui::TextUnformatted(NodeTypeHelpText(Type));
            ImGui::PopTextWrapPos();
        }
        ImGui::End();
    }

    const char* PinTypeLabel(ELuaBlueprintPinType Type)
    {
        switch (Type)
        {
        case ELuaBlueprintPinType::Exec:
            return "Exec";
        case ELuaBlueprintPinType::Bool:
            return "Bool";
        case ELuaBlueprintPinType::Int:
            return "Int";
        case ELuaBlueprintPinType::Float:
            return "Float";
        case ELuaBlueprintPinType::String:
            return "String";
        case ELuaBlueprintPinType::Vector:
            return "Vector";
        case ELuaBlueprintPinType::Object:
            return "Object";
        case ELuaBlueprintPinType::Any:
            return "Any";
        case ELuaBlueprintPinType::Array:
            return "Array";
        }
        return "Unknown";
    }

    const char* SeverityLabel(ELuaBlueprintDiagnosticSeverity Severity)
    {
        switch (Severity)
        {
        case ELuaBlueprintDiagnosticSeverity::Info:
            return "Info";
        case ELuaBlueprintDiagnosticSeverity::Warning:
            return "Warning";
        case ELuaBlueprintDiagnosticSeverity::Error:
            return "Error";
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
        case ELuaBlueprintNodeType::ForEachArray:
        case ELuaBlueprintNodeType::Delay:
            return ImVec4(0.95f, 0.80f, 0.35f, 1.0f);
        case ELuaBlueprintNodeType::CallFunction:
        case ELuaBlueprintNodeType::CallFunctionSignature:
        case ELuaBlueprintNodeType::CallCustomEvent:
        case ELuaBlueprintNodeType::CustomEvent:
            return ImVec4(0.45f, 0.70f, 1.00f, 1.0f);
        case ELuaBlueprintNodeType::BindEvent:
        case ELuaBlueprintNodeType::UnbindEvent:
        case ELuaBlueprintNodeType::HasEventBinding:
            return ImVec4(0.95f, 0.55f, 0.95f, 1.0f);
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
        case ELuaBlueprintPinType::Exec:
            return ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
        case ELuaBlueprintPinType::Bool:
            return ImVec4(0.95f, 0.30f, 0.30f, 1.0f);
        case ELuaBlueprintPinType::Int:
            return ImVec4(0.45f, 0.95f, 0.85f, 1.0f);
        case ELuaBlueprintPinType::Float:
            return ImVec4(0.55f, 0.95f, 0.45f, 1.0f);
        case ELuaBlueprintPinType::String:
            return ImVec4(0.95f, 0.45f, 0.85f, 1.0f);
        case ELuaBlueprintPinType::Vector:
            return ImVec4(0.95f, 0.85f, 0.30f, 1.0f);
        case ELuaBlueprintPinType::Object:
            return ImVec4(0.40f, 0.75f, 1.00f, 1.0f);
        case ELuaBlueprintPinType::Any:
            return ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
        case ELuaBlueprintPinType::Array:
            return ImVec4(0.55f, 0.60f, 1.00f, 1.0f);
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
                if (std::tolower(static_cast<unsigned char>(Haystack[i + j])) != std::tolower(
                    static_cast<unsigned char>(Needle[j])
                ))
                {
                    bMatch = false;
                    break;
                }
            }
            if (bMatch) return true;
        }
        return false;
    }

    ELuaBlueprintPinType NormalizeVariablePinTypeForEditor(ELuaBlueprintPinType Type)
    {
        switch (Type)
        {
        case ELuaBlueprintPinType::Bool:
        case ELuaBlueprintPinType::Int:
        case ELuaBlueprintPinType::Float:
        case ELuaBlueprintPinType::String:
        case ELuaBlueprintPinType::Vector:
        case ELuaBlueprintPinType::Object:
        case ELuaBlueprintPinType::Array:
            return Type;
        default:
            return ELuaBlueprintPinType::Float;
        }
    }

    int VariablePinTypeToComboIndex(ELuaBlueprintPinType Type)
    {
        switch (NormalizeVariablePinTypeForEditor(Type))
        {
        case ELuaBlueprintPinType::Bool:   return 0;
        case ELuaBlueprintPinType::Int:    return 1;
        case ELuaBlueprintPinType::Float:  return 2;
        case ELuaBlueprintPinType::String: return 3;
        case ELuaBlueprintPinType::Vector: return 4;
        case ELuaBlueprintPinType::Object: return 5;
        case ELuaBlueprintPinType::Array:  return 6;
        default:                           return 2;
        }
    }

    ELuaBlueprintPinType ComboIndexToVariablePinType(int Index)
    {
        switch (Index)
        {
        case 0:  return ELuaBlueprintPinType::Bool;
        case 1:  return ELuaBlueprintPinType::Int;
        case 2:  return ELuaBlueprintPinType::Float;
        case 3:  return ELuaBlueprintPinType::String;
        case 4:  return ELuaBlueprintPinType::Vector;
        case 5:  return ELuaBlueprintPinType::Object;
        case 6:  return ELuaBlueprintPinType::Array;
        default: return ELuaBlueprintPinType::Float;
        }
    }

    FName MakeUniqueVariableNameForEditor(const ULuaBlueprintAsset* Blueprint, const FName& DesiredName, int32 CurrentIndex)
    {
        if (!Blueprint) return DesiredName == FName::None ? FName("Variable") : DesiredName;

        FString BaseName = DesiredName == FName::None ? FString("Variable") : DesiredName.ToString();
        if (BaseName.empty()) BaseName = "Variable";

        FString Candidate = BaseName;
        int32 Suffix = 1;
        bool bUnique = false;
        while (!bUnique)
        {
            bUnique = true;
            const TArray<FLuaBlueprintVariable>& Variables = Blueprint->GetVariables();
            for (int32 i = 0; i < static_cast<int32>(Variables.size()); ++i)
            {
                if (i == CurrentIndex) continue;
                if (Variables[i].Name.ToString() == Candidate)
                {
                    Candidate = BaseName + std::to_string(Suffix++);
                    bUnique = false;
                    break;
                }
            }
        }
        return FName(Candidate);
    }

    struct FLuaBlueprintNodeFragmentBounds
    {
        ImVec2 Min = ImVec2(0.0f, 0.0f);
        ImVec2 Max = ImVec2(0.0f, 0.0f);
        bool   bValid = false;
    };

    ImVec2 EstimateLuaBlueprintNodeSize(const FLuaBlueprintNode& Node)
    {
        if (Node.Type == ELuaBlueprintNodeType::Comment)
        {
            return ImVec2(std::max(80.0f, Node.VectorValue.X), std::max(40.0f, Node.VectorValue.Y));
        }

        return ImVec2(180.0f, 90.0f);
    }

    FLuaBlueprintNodeFragmentBounds ComputeNodeFragmentBounds(
        const TArray<FLuaBlueprintNode>& Nodes,
        bool bUseEditorNodeSizes,
        bool bPreferNonCommentNodes = false
        )
    {
        FLuaBlueprintNodeFragmentBounds Bounds;
        if (Nodes.empty()) return Bounds;

        bool bHasNonComment = false;
        if (bPreferNonCommentNodes)
        {
            for (const FLuaBlueprintNode& Node : Nodes)
            {
                if (Node.Type != ELuaBlueprintNodeType::Comment)
                {
                    bHasNonComment = true;
                    break;
                }
            }
        }

        float MinX = FLT_MAX;
        float MinY = FLT_MAX;
        float MaxX = -FLT_MAX;
        float MaxY = -FLT_MAX;

        for (const FLuaBlueprintNode& Node : Nodes)
        {
            if (bHasNonComment && Node.Type == ELuaBlueprintNodeType::Comment)
            {
                continue;
            }

            ImVec2 Pos(Node.PosX, Node.PosY);
            ImVec2 Size = EstimateLuaBlueprintNodeSize(Node);
            if (bUseEditorNodeSizes)
            {
                const ImVec2 EditorSize = ed::GetNodeSize(ToNodeId(Node.NodeId));
                if (EditorSize.x > 1.0f && EditorSize.y > 1.0f)
                {
                    Size = EditorSize;
                }
            }

            MinX = std::min(MinX, Pos.x);
            MinY = std::min(MinY, Pos.y);
            MaxX = std::max(MaxX, Pos.x + Size.x);
            MaxY = std::max(MaxY, Pos.y + Size.y);
            Bounds.bValid = true;
        }

        if (!Bounds.bValid) return Bounds;
        Bounds.Min = ImVec2(MinX, MinY);
        Bounds.Max = ImVec2(MaxX, MaxY);
        return Bounds;
    }

    FLuaBlueprintNodeFragmentBounds ComputeLiveNodeBounds(const FLuaBlueprintNode& Node, bool bUseEditorNode)
    {
        FLuaBlueprintNodeFragmentBounds Bounds;
        ImVec2 Pos(Node.PosX, Node.PosY);
        ImVec2 Size = EstimateLuaBlueprintNodeSize(Node);
        if (bUseEditorNode)
        {
            Pos = ed::GetNodePosition(ToNodeId(Node.NodeId));
            const ImVec2 EditorSize = ed::GetNodeSize(ToNodeId(Node.NodeId));
            if (EditorSize.x > 1.0f && EditorSize.y > 1.0f)
            {
                Size = EditorSize;
            }
        }
        Bounds.Min = Pos;
        Bounds.Max = ImVec2(Pos.x + Size.x, Pos.y + Size.y);
        Bounds.bValid = true;
        return Bounds;
    }

    bool IsBoundsFullyInside(const FLuaBlueprintNodeFragmentBounds& Inner, const FLuaBlueprintNodeFragmentBounds& Outer)
    {
        if (!Inner.bValid || !Outer.bValid) return false;
        constexpr float Tolerance = 1.0f;
        return Inner.Min.x >= Outer.Min.x - Tolerance
            && Inner.Min.y >= Outer.Min.y - Tolerance
            && Inner.Max.x <= Outer.Max.x + Tolerance
            && Inner.Max.y <= Outer.Max.y + Tolerance;
    }

    ImVec2 ComputeNodeFragmentMin(const TArray<FLuaBlueprintNode>& Nodes)
    {
        const FLuaBlueprintNodeFragmentBounds Bounds = ComputeNodeFragmentBounds(Nodes, false);
        return Bounds.bValid ? Bounds.Min : ImVec2(0.0f, 0.0f);
    }

    FLuaBlueprintNodeFragmentBounds ComputeNodeFragmentPasteReferenceBounds(
        const TArray<FLuaBlueprintNode>& Nodes,
        bool bUseEditorNodeSizes
        )
    {
        // Duplicate/paste placement should be based on actual source nodes, not on an oversized
        // selected comment/group rectangle. If only comments are selected, comments become the basis.
        return ComputeNodeFragmentBounds(Nodes, bUseEditorNodeSizes, true);
    }


    bool IsValueProducingNode(ELuaBlueprintNodeType Type)
    {
        switch (Type)
        {
        case ELuaBlueprintNodeType::LiteralBool:
        case ELuaBlueprintNodeType::LiteralInt:
        case ELuaBlueprintNodeType::LiteralFloat:
        case ELuaBlueprintNodeType::LiteralString:
        case ELuaBlueprintNodeType::LiteralVector:
        case ELuaBlueprintNodeType::GetVariable:
        case ELuaBlueprintNodeType::GetProperty:
        case ELuaBlueprintNodeType::CallFunction:
        case ELuaBlueprintNodeType::CallFunctionSignature:
        case ELuaBlueprintNodeType::Self:
        case ELuaBlueprintNodeType::FindActorByName:
        case ELuaBlueprintNodeType::FindActorByClass:
        case ELuaBlueprintNodeType::FindActorByTag:
        case ELuaBlueprintNodeType::FindActorsByTag:
        case ELuaBlueprintNodeType::FindActorsByClass:
        case ELuaBlueprintNodeType::GetActorLocation:
        case ELuaBlueprintNodeType::GetActorRotation:
        case ELuaBlueprintNodeType::GetActorScale:
        case ELuaBlueprintNodeType::GetActorForward:
        case ELuaBlueprintNodeType::GetActorRight:
        case ELuaBlueprintNodeType::ActorHasTag:
        case ELuaBlueprintNodeType::GetActorName:
        case ELuaBlueprintNodeType::GetOwnerActor:
        case ELuaBlueprintNodeType::IsValid:
        case ELuaBlueprintNodeType::GetRootComponent:
        case ELuaBlueprintNodeType::GetComponentByName:
        case ELuaBlueprintNodeType::GetPrimitiveComponent:
        case ELuaBlueprintNodeType::GetLinearVelocity:
        case ELuaBlueprintNodeType::GetMass:
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
        case ELuaBlueprintNodeType::GetGameTime:
        case ELuaBlueprintNodeType::Reroute:
        case ELuaBlueprintNodeType::ToBool:
        case ELuaBlueprintNodeType::ToInt:
        case ELuaBlueprintNodeType::ToFloat:
        case ELuaBlueprintNodeType::ToString:
        case ELuaBlueprintNodeType::ToVector:
            return true;
        default:
            return false;
        }
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
    bPendingInitialContentFit = true;
    bPendingNodeGeometryEdit = false;

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
        CaptureInitialUndoSnapshot(Blueprint);
    }
}

void FLuaBlueprintEditorWidget::Close()
{
    if (bPendingNodeGeometryEdit)
    {
        if (ULuaBlueprintAsset* Blueprint = GetBlueprint())
        {
            CommitBlueprintEdit(Blueprint);
        }
        bPendingNodeGeometryEdit = false;
    }

    DestroyContext();
    bPositionsPushed = false;
    UndoStack.clear();
    RedoStack.clear();
    ClipboardNodes.clear();
    ClipboardLinks.clear();
    bPendingInitialContentFit = false;
    bPendingNodeGeometryEdit = false;
    FAssetEditorWidget::Close();
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

    // 표시 라벨은 dirty mark(*) 때문에 바뀔 수 있지만, ### 뒤 ID 는 자산 인스턴스로 고정한다.
    // ## 는 표시 suffix 만 숨길 뿐 앞 라벨까지 ID 에 포함되므로 dirty toggle 시 다른 창으로 취급될 수 있다.
    const FString DisplayLabel = (Blueprint->GetSourcePath().empty() ? Blueprint->GetName() : Blueprint->GetSourcePath());
    const FString DirtyMark    = (IsDirty() || Blueprint->IsCompileDirty()) ? FString("*") : FString();
    char WindowTitleBuf[512];
    std::snprintf(
        WindowTitleBuf,
        sizeof(WindowTitleBuf),
        "Lua Blueprint - %s%s###LuaBP_%p",
        DisplayLabel.c_str(),
        DirtyMark.c_str(),
        static_cast<const void*>(Blueprint)
    );

    bool bWindowOpen = IsOpen();
    ImGui::SetNextWindowSize(ImVec2(1440.0f, 900.0f), ImGuiCond_Once);
    if (!ImGui::Begin(WindowTitleBuf, &bWindowOpen, ImGuiWindowFlags_MenuBar))
    {
        ImGui::End();
        if (!bWindowOpen) Close();
        return;
    }

    if (ConsumeFocusRequest())
    {
        ImGui::SetWindowFocus();
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        ImGuiIO& IO = ImGui::GetIO();
        const bool bCtrl = IO.KeyCtrl || IO.KeySuper;
        if (bCtrl && ImGui::IsKeyPressed(ImGuiKey_Z))
        {
            UndoBlueprintEdit(Blueprint);
        }
        else if (bCtrl && ImGui::IsKeyPressed(ImGuiKey_Y))
        {
            RedoBlueprintEdit(Blueprint);
        }
        else if (!IO.WantTextInput && bCtrl && ImGui::IsKeyPressed(ImGuiKey_C))
        {
            bQueuedCopySelected = true;
        }
        else if (!IO.WantTextInput && bCtrl && ImGui::IsKeyPressed(ImGuiKey_V))
        {
            bQueuedPasteNodes = true;
        }
        else if (!IO.WantTextInput && bCtrl && ImGui::IsKeyPressed(ImGuiKey_D))
        {
            bQueuedDuplicateSelected = true;
        }
        else if (!IO.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete))
        {
            bQueuedDeleteSelected = true;
        }
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

void FLuaBlueprintEditorWidget::EnsureContext()
{
    if (NodeEditorContext) return;
    ed::Config Cfg;
    Cfg.SettingsFile  = nullptr;
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

ULuaBlueprintAsset* FLuaBlueprintEditorWidget::GetBlueprint() const
{
    return Cast<ULuaBlueprintAsset>(EditedObject);
}

namespace
{
    TArray<uint8> CaptureLuaBlueprintSnapshot(ULuaBlueprintAsset* Blueprint)
    {
        TArray<uint8> Buffer;
        if (!Blueprint) return Buffer;
        FMemoryArchive Saver(true);
        Blueprint->Serialize(Saver);
        Buffer = Saver.GetBuffer();
        return Buffer;
    }
}

void FLuaBlueprintEditorWidget::RenderToolbar(ULuaBlueprintAsset* Blueprint)
{
    if (!ImGui::BeginMenuBar()) return;

    const bool bDirtyNow = IsDirty() || Blueprint->IsCompileDirty();
    // dirty 면 Save 버튼 강조 — Material editor 패턴.
    if (bDirtyNow) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.85f, 0.35f, 1.0f));
    if (ImGui::MenuItem(bDirtyNow ? "Save*" : "Save"))
    {
        // Compile errors must not block graph persistence; runtime will keep using last-good Lua.
        Blueprint->Compile();
        if (FLuaBlueprintManager::Get().Save(Blueprint))
        {
            ClearDirty();
        }
    }
    if (bDirtyNow) ImGui::PopStyleColor();

    if (ImGui::MenuItem("Compile"))
    {
        Blueprint->Compile();
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Undo", "Ctrl+Z", false, UndoStack.size() > 1))
    {
        UndoBlueprintEdit(Blueprint);
    }
    if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !RedoStack.empty()))
    {
        RedoBlueprintEdit(Blueprint);
    }
    if (ImGui::MenuItem("Copy", "Ctrl+C"))
    {
        bQueuedCopySelected = true;
    }
    if (ImGui::MenuItem("Paste", "Ctrl+V", false, !ClipboardNodes.empty()))
    {
        bQueuedPasteNodes = true;
    }
    if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
    {
        bQueuedDuplicateSelected = true;
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Reset Default"))
    {
        Blueprint->InitializeDefault();
        bPositionsPushed = false;
        CommitBlueprintEdit(Blueprint);
    }
    ImGui::Separator();
    ImGui::TextDisabled("Right-click canvas for menu. Drag pins to link. Drag a variable to canvas for Get/Set.");

    ImGui::EndMenuBar();
}

void FLuaBlueprintEditorWidget::RenderCompileErrorPanel(ULuaBlueprintAsset* Blueprint)
{
    // Material 패턴: 상단에 빨간 에러 패널을 명시적으로 노출. Diagnostics 탭은 보조용.
    bool bHasError   = false;
    int  NumWarnings = 0;
    for (const FLuaBlueprintDiagnostic& D : Blueprint->GetDiagnostics())
    {
        if (D.Severity == ELuaBlueprintDiagnosticSeverity::Error) bHasError = true;
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
        AddVariableMenuItem(Blueprint, ELuaBlueprintPinType::Array, "Array");
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
        const bool    bOpen       = ImGui::TreeNodeEx("##Var", ImGuiTreeNodeFlags_DefaultOpen, " ");
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

void FLuaBlueprintEditorWidget::RenderGraph(ULuaBlueprintAsset* Blueprint)
{
    const float InspectorWidth = 360.0f;
    const float Spacing = ImGui::GetStyle().ItemSpacing.x;
    const float TotalWidth = ImGui::GetContentRegionAvail().x;
    const float CanvasWidth = (TotalWidth > InspectorWidth + Spacing + 120.0f) ? TotalWidth - InspectorWidth - Spacing
    : TotalWidth;

    uint32 SelectedNodeId = 0;

    ImGui::BeginChild("##LuaBlueprintCanvasChild", ImVec2(CanvasWidth, 0), ImGuiChildFlags_Borders);

    const ImVec2 TooltipOwnerMin = ImGui::GetWindowPos();
    const ImVec2 TooltipOwnerMax(
        TooltipOwnerMin.x + ImGui::GetWindowSize().x,
        TooltipOwnerMin.y + ImGui::GetWindowSize().y
    );
    const ImRect TooltipOwnerRect(TooltipOwnerMin, TooltipOwnerMax);
    const ImGuiViewport* TooltipOwnerViewport = ImGui::GetWindowViewport();
    const ImGuiID TooltipOwnerViewportId = TooltipOwnerViewport ? TooltipOwnerViewport->ID : 0;

    ed::SetCurrentEditor(NodeEditorContext);
    ed::Begin("LuaBlueprintCanvas");

    // 이전 프레임에 변수 drop 이 들어왔다면 이제 ed 컨텍스트가 활성 상태이므로
    // 안전하게 screen→canvas 변환을 끝내고 Get/Set 팝업을 띄울 준비.
    if (bPendingVariableDrop)
    {
        PendingVariableDropPos = ed::ScreenToCanvas(PendingVariableScreenPos);
        bPendingVariableDrop   = false;
        bShowVariableDropMenu  = true;
    }

    if (!bPositionsPushed)
    {
        for (const FLuaBlueprintNode& Node : Blueprint->GetNodes())
        {
            ed::SetNodePosition(ToNodeId(Node.NodeId), ImVec2(Node.PosX, Node.PosY));
        }
        bPositionsPushed = true;
    }

    ProcessQueuedNodeEditorCommands(Blueprint);

    bool bAnyNodeGeometryChangedThisFrame = false;
    bool bHoveredNodeHelpIcon = false;
    ELuaBlueprintNodeType HoveredNodeHelpType = ELuaBlueprintNodeType::Comment;

    for (FLuaBlueprintNode& Node : Blueprint->GetMutableNodes())
    {
        // Comment 는 ed::Group 으로 노출 — 내부에 겹친 다른 노드들을 같이 드래그함 (UE BP Comment).
        if (Node.Type == ELuaBlueprintNodeType::Comment)
        {
            ed::PushStyleColor(ed::StyleColor_NodeBg,     ImColor(110, 95, 30, 80));
            ed::PushStyleColor(ed::StyleColor_NodeBorder, ImColor(220, 200, 100, 200));

            ed::BeginNode(ToNodeId(Node.NodeId));
            ImGui::TextColored(ImVec4(1.0f, 0.95f, 0.5f, 1.0f), "%s", Node.StringValue.empty() ? "Comment" : Node.StringValue.c_str());
            if (RenderNodeHelpIcon(Node.Type, HoveredNodeHelpType))
            {
                bHoveredNodeHelpIcon = true;
            }
            const ImVec2 GroupSize(std::max(80.0f, Node.VectorValue.X), std::max(40.0f, Node.VectorValue.Y));
            ed::Group(GroupSize);
            ed::EndNode();

            // 사용자가 ed 측 핸들로 리사이즈한 경우 VectorValue 도 동기화.
            const ImVec2 ActualSize = ed::GetNodeSize(ToNodeId(Node.NodeId));
            if (ActualSize.x > 0 && ActualSize.y > 0)
            {
                if (std::fabs(Node.VectorValue.X - ActualSize.x) > 0.5f || std::fabs(Node.VectorValue.Y - ActualSize.y) > 0.5f)
                {
                    Node.VectorValue.X = ActualSize.x;
                    Node.VectorValue.Y = ActualSize.y;
                    Blueprint->BumpEditorVersion();
                    bAnyNodeGeometryChangedThisFrame = true;
                }
            }

            ed::PopStyleColor(2);
            continue;
        }

        ed::BeginNode(ToNodeId(Node.NodeId));
        ImGui::TextColored(NodeHeaderColor(Node.Type), "%s", NodeTypeLabel(Node.Type));
        if (RenderNodeHelpIcon(Node.Type, HoveredNodeHelpType))
        {
            bHoveredNodeHelpIcon = true;
        }
        ImGui::Dummy(ImVec2(0.0f, 2.0f));

        RenderNodeBody(Blueprint, Node);

        for (FLuaBlueprintPin& Pin : Node.Pins)
        {
            ed::BeginPin(
                ToPinId(Pin.PinId),
                Pin.Kind == ELuaBlueprintPinKind::Input ? ed::PinKind::Input : ed::PinKind::Output
            );
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

            // Input pin 옆에 연결 상태/자동 형변환 badge와 inline literal editor를 표시한다.
            if (Pin.Kind == ELuaBlueprintPinKind::Input)
            {
                ImGui::SameLine();
                RenderInputPinConnectionStatus(Blueprint, Pin);
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

    // SetNodePosition 직후가 아니라, 실제 노드들이 이번 frame 에 제출된 뒤 fit 해야
    // node-editor 내부 bounds 가 살아있는 노드 기준으로 계산된다.
    if (bPendingInitialContentFit && !Blueprint->GetNodes().empty())
    {
        ed::NavigateToContent(0.0f);
        bPendingInitialContentFit = false;
    }

    if (ed::BeginCreate())
    {
        ed::PinId StartId, EndId;
        if (ed::QueryNewLink(&StartId, &EndId))
        {
            if (StartId && EndId)
            {
                uint32 FromPinId    = 0;
                uint32 ToPinIdValue = 0;
                if (Blueprint->CanLinkPins(PinIdToU32(StartId), PinIdToU32(EndId), &FromPinId, &ToPinIdValue))
                {
                    if (ed::AcceptNewItem())
                    {
                        Blueprint->AddLink(FromPinId, ToPinIdValue);
                        CommitBlueprintEdit(Blueprint);
                    }
                }
                else
                {
                    ed::RejectNewItem(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), 2.0f);
                }
            }
        }

        // 핀을 빈 공간으로 드래그해 놓는 경우는 QueryNewLink(Start, End)만으로는
        // 안정적으로 잡히지 않는다. ax::NodeEditor의 QueryNewNode 경로를 사용해야
        // release 시점에 context-sensitive node popup을 열 수 있다.
        ed::PinId NewNodePinId = 0;
        if (ed::QueryNewNode(&NewNodePinId))
        {
            if (NewNodePinId && ed::AcceptNewItem(ImVec4(0.45f, 0.75f, 1.0f, 1.0f), 1.5f))
            {
                PendingPinSpawnPinId = PinIdToU32(NewNodePinId);
                PendingPinSpawnPos   = ed::ScreenToCanvas(ImGui::GetMousePos());
                PendingNewNodePosition = PendingPinSpawnPos;
                PinSpawnSearchBuf[0] = 0;
                bShowPinSpawnMenu = true;
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
                if (Blueprint->RemoveLink(LinkIdToU32(DeletedLink))) CommitBlueprintEdit(Blueprint);
            }
        }

        TArray<uint32> DeletedNodeIds;
        ed::NodeId DeletedNode;
        while (ed::QueryDeletedNode(&DeletedNode))
        {
            if (ed::AcceptDeletedItem())
            {
                DeletedNodeIds.push_back(NodeIdToU32(DeletedNode));
            }
        }
        if (!DeletedNodeIds.empty() && DeleteNodesIncludingContainedGroups(Blueprint, DeletedNodeIds))
        {
            CommitBlueprintEdit(Blueprint);
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
            Blueprint->BumpEditorVersion();
            bAnyNodeGeometryChangedThisFrame = true;
        }
    }

    // Node-editor drag/resize updates can arrive every frame. Keep the data model live so hit tests,
    // grouping and saving use current coordinates, but push only one undo snapshot when the drag ends.
    const bool bMouseDownForGeometryDrag = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    if (bAnyNodeGeometryChangedThisFrame)
    {
        bPendingNodeGeometryEdit = true;
        if (!bMouseDownForGeometryDrag)
        {
            CommitBlueprintEdit(Blueprint);
            bPendingNodeGeometryEdit = false;
        }
    }
    else if (bPendingNodeGeometryEdit && !bMouseDownForGeometryDrag)
    {
        CommitBlueprintEdit(Blueprint);
        bPendingNodeGeometryEdit = false;
    }

    ed::NodeId ContextNodeId = 0;
    ed::PinId  ContextPinId  = 0;
    ed::LinkId ContextLinkId = 0;

    ed::Suspend();
    if (ed::ShowNodeContextMenu(&ContextNodeId))
    {
        ImGui::OpenPopup("LuaBlueprintNodeMenu");
    }
    else if (ed::ShowPinContextMenu(&ContextPinId))
    {
        PendingPinSpawnPinId = PinIdToU32(ContextPinId);
        PendingPinSpawnPos = ed::ScreenToCanvas(ImGui::GetMousePos());
        PendingNewNodePosition = PendingPinSpawnPos;
        PinSpawnSearchBuf[0] = 0;
        ImGui::OpenPopup("LuaBlueprintPinMenu");
    }
    else if (ed::ShowLinkContextMenu(&ContextLinkId))
    {
        ImGui::OpenPopup("LuaBlueprintLinkMenu");
    }
    else if (ed::ShowBackgroundContextMenu())
    {
        PendingNewNodePosition = ed::ScreenToCanvas(ImGui::GetMousePos());
        AddNodeSearchBuf[0]    = 0;
        ImGui::OpenPopup("LuaBlueprintBackgroundMenu");
    }

    if (ImGui::BeginPopup("LuaBlueprintNodeMenu"))
    {
        if (ImGui::MenuItem("Copy"))
        {
            bQueuedCopySelected = true;
        }
        if (ImGui::MenuItem("Duplicate"))
        {
            bQueuedDuplicateSelected = true;
        }
        // 다중 선택 확인은 ed::GetSelectedNodes 가 ed 컨텍스트를 요구하므로
        // 정확한 개수 체크 대신 메뉴는 항상 노출하고 핸들러에서 1개 미만이면 no-op.
        if (ImGui::MenuItem("Group Selection as Comment"))
        {
            bQueuedGroupSelected = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete"))
        {
            TArray<uint32> RootNodeIds;
            RootNodeIds.push_back(NodeIdToU32(ContextNodeId));
            if (DeleteNodesIncludingContainedGroups(Blueprint, RootNodeIds)) CommitBlueprintEdit(Blueprint);
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("LuaBlueprintLinkMenu"))
    {
        if (ImGui::MenuItem("Break Link"))
        {
            if (Blueprint->RemoveLink(LinkIdToU32(ContextLinkId))) CommitBlueprintEdit(Blueprint);
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("LuaBlueprintPinMenu"))
    {
        if (ContextPinId)
        {
            PendingPinSpawnPinId = PinIdToU32(ContextPinId);
            PendingPinSpawnPos = ed::ScreenToCanvas(ImGui::GetMousePos());
            PendingNewNodePosition = PendingPinSpawnPos;
        }
        RenderPinSpawnMenu(Blueprint);
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("LuaBlueprintBackgroundMenu"))
    {
        if (!ClipboardNodes.empty() && ImGui::MenuItem("Paste", "Ctrl+V"))
        {
            PasteCopiedNodes(Blueprint, &PendingNewNodePosition);
        }
        if (ImGui::MenuItem("Group Selection as Comment"))
        {
            bQueuedGroupSelected = true;
        }
        ImGui::Separator();
        RenderAddNodeMenu(Blueprint);
        ImGui::EndPopup();
    }

    if (bShowPinSpawnMenu)
    {
        ImGui::OpenPopup("LuaBlueprintPinSpawnMenu");
        bShowPinSpawnMenu = false;
    }
    if (ImGui::BeginPopup("LuaBlueprintPinSpawnMenu"))
    {
        RenderPinSpawnMenu(Blueprint);
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
            SpawnVariableNode(
                Blueprint,
                ELuaBlueprintNodeType::GetVariable,
                PendingVariableDropName,
                PendingVariableDropPos
            );
        }
        if (ImGui::MenuItem("Set"))
        {
            SpawnVariableNode(
                Blueprint,
                ELuaBlueprintNodeType::SetVariable,
                PendingVariableDropName,
                PendingVariableDropPos
            );
        }
        ImGui::EndPopup();
    }
    ed::Resume();

    {
        ed::NodeId SelectedNodes[1];
        const int  SelectedCount = ed::GetSelectedNodes(SelectedNodes, 1);
        if (SelectedCount > 0)
        {
            SelectedNodeId = NodeIdToU32(SelectedNodes[0]);
        }
    }

    ed::End();
    ed::SetCurrentEditor(nullptr);

    if (bHoveredNodeHelpIcon)
    {
        RenderNodeHelpTooltip(HoveredNodeHelpType, TooltipOwnerRect, TooltipOwnerViewportId);
    }

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

void FLuaBlueprintEditorWidget::RenderNodeBody(ULuaBlueprintAsset* Blueprint, FLuaBlueprintNode& Node)
{
    // Material editor 가 노드 본문에 색 swatch / 텍스처 썸네일을 그리는 것과 같은 자리.
    // LuaBP literal 노드는 인라인으로 값 편집기를 표시 — 노드 안에서 바로 수정 가능.
    switch (Node.Type)
    {
    case ELuaBlueprintNodeType::LiteralBool:
        ImGui::PushID(static_cast<int>(Node.NodeId));
        if (ImGui::Checkbox("##lb", &Node.BoolValue))
        {
            Blueprint->BumpVersion();
            CommitBlueprintEdit(Blueprint);
        }
        ImGui::PopID();
        break;
    case ELuaBlueprintNodeType::LiteralInt:
        ImGui::PushID(static_cast<int>(Node.NodeId));
        ImGui::SetNextItemWidth(80.0f);
        if (ImGui::InputInt("##li", &Node.IntValue, 0))
        {
            Blueprint->BumpVersion();
            CommitBlueprintEdit(Blueprint);
        }
        ImGui::PopID();
        break;
    case ELuaBlueprintNodeType::LiteralFloat:
        ImGui::PushID(static_cast<int>(Node.NodeId));
        ImGui::SetNextItemWidth(80.0f);
        if (ImGui::DragFloat("##lf", &Node.FloatValue, 0.01f, 0.0f, 0.0f, "%.3f"))
        {
            Blueprint->BumpVersion();
            CommitBlueprintEdit(Blueprint);
        }
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
            CommitBlueprintEdit(Blueprint);
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
            CommitBlueprintEdit(Blueprint);
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
    case ELuaBlueprintNodeType::CustomEvent:
    case ELuaBlueprintNodeType::CallCustomEvent:
        ImGui::TextDisabled(Node.NameValue == FName::None ? "(no event)" : Node.NameValue.ToString().c_str());
        break;
    case ELuaBlueprintNodeType::Comment:
        ImGui::TextWrapped("%s", Node.StringValue.empty() ? "Comment" : Node.StringValue.c_str());
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
    case ELuaBlueprintNodeType::BindEvent:
        ImGui::TextDisabled(Node.NameValue == FName::None ? "(no event)" : Node.NameValue.ToString().c_str());
        if (!Node.StringValue.empty()) ImGui::TextDisabled("→ %s", Node.StringValue.c_str());
        break;
    case ELuaBlueprintNodeType::UnbindEvent:
    case ELuaBlueprintNodeType::HasEventBinding:
        ImGui::TextDisabled(Node.NameValue == FName::None ? "(no event)" : Node.NameValue.ToString().c_str());
        break;
    default:
        break;
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
        CommitBlueprintEdit(Blueprint);
    }

    switch (Node.Type)
    {
    case ELuaBlueprintNodeType::GetVariable:
    case ELuaBlueprintNodeType::SetVariable:
    {
        FString     CurrentName = Node.NameValue.ToString();
        const char* Preview     = CurrentName.empty() ? "(none)" : CurrentName.c_str();
        if (ImGui::BeginCombo("Variable", Preview))
        {
            for (const FLuaBlueprintVariable& Variable : Blueprint->GetVariables())
            {
                const FString VarName   = Variable.Name.ToString();
                const bool    bSelected = (VarName == CurrentName);
                if (ImGui::Selectable(VarName.c_str(), bSelected))
                {
                    Node.NameValue = Variable.Name;
                    Blueprint->RefreshNodePinTypes(Node);
                    Blueprint->BumpVersion();
                    CommitBlueprintEdit(Blueprint);
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
            Blueprint->RefreshNodePinTypes(Node);
            Blueprint->BumpVersion();
            CommitBlueprintEdit(Blueprint);
        }
        break;
    }
    case ELuaBlueprintNodeType::GetProperty:
    case ELuaBlueprintNodeType::SetProperty:
    case ELuaBlueprintNodeType::CallFunction:
    {
        char NameBuf[160];
        CopyToBuffer(NameBuf, sizeof(NameBuf), Node.NameValue.ToString());
        if (ImGui::InputText(
            Node.Type == ELuaBlueprintNodeType::CallFunction ? "Function" : "Property",
            NameBuf,
            sizeof(NameBuf),
            ImGuiInputTextFlags_EnterReturnsTrue
        ))
        {
            Node.NameValue = NameBuf[0] ? FName(NameBuf) : FName::None;
            Blueprint->BumpVersion();
            CommitBlueprintEdit(Blueprint);
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
            CommitBlueprintEdit(Blueprint);
        }
        break;
    }
    case ELuaBlueprintNodeType::CustomEvent:
    case ELuaBlueprintNodeType::CallCustomEvent:
    {
        char EventBuf[160];
        CopyToBuffer(EventBuf, sizeof(EventBuf), Node.NameValue.ToString());
        if (ImGui::InputText("Event Name", EventBuf, sizeof(EventBuf), ImGuiInputTextFlags_EnterReturnsTrue))
        {
            Node.NameValue = EventBuf[0] ? FName(EventBuf) : FName::None;
            Blueprint->BumpVersion();
            CommitBlueprintEdit(Blueprint);
        }
        break;
    }
    case ELuaBlueprintNodeType::BindEvent:
    case ELuaBlueprintNodeType::UnbindEvent:
    case ELuaBlueprintNodeType::HasEventBinding:
    {
        // target 의 함수/시그니처 이름 — Reflection.BindEvent 2번째 인자.
        char EventBuf[160];
        CopyToBuffer(EventBuf, sizeof(EventBuf), Node.NameValue.ToString());
        if (ImGui::InputText("Target Event", EventBuf, sizeof(EventBuf), ImGuiInputTextFlags_EnterReturnsTrue))
        {
            Node.NameValue = EventBuf[0] ? FName(EventBuf) : FName::None;
            Blueprint->BumpVersion();
            CommitBlueprintEdit(Blueprint);
        }
        if (Node.Type == ELuaBlueprintNodeType::BindEvent)
        {
            // 우리 그래프의 CustomEvent 이름 — 콜백으로 매핑됨. dropdown + 자유 입력.
            char CallbackBuf[160];
            CopyToBuffer(CallbackBuf, sizeof(CallbackBuf), Node.StringValue);
            const FString Preview = Node.StringValue.empty() ? FString("(none)") : Node.StringValue;
            if (ImGui::BeginCombo("Callback", Preview.c_str()))
            {
                for (const FLuaBlueprintNode& Other : Blueprint->GetNodes())
                {
                    if (Other.Type != ELuaBlueprintNodeType::CustomEvent) continue;
                    const FString EvName = Other.NameValue.ToString();
                    if (EvName.empty()) continue;
                    const bool bSel = (Node.StringValue == EvName);
                    if (ImGui::Selectable(EvName.c_str(), bSel))
                    {
                        Node.StringValue = EvName;
                        Blueprint->BumpVersion();
                        CommitBlueprintEdit(Blueprint);
                    }
                    if (bSel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (ImGui::InputText("Callback Name", CallbackBuf, sizeof(CallbackBuf), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                Node.StringValue = CallbackBuf;
                Blueprint->BumpVersion();
                CommitBlueprintEdit(Blueprint);
            }
            ImGui::TextDisabled("(목록의 Custom Event 이름이나 직접 입력)");
        }
        break;
    }
    case ELuaBlueprintNodeType::Comment:
    {
        char TextBuf[512];
        CopyToBuffer(TextBuf, sizeof(TextBuf), Node.StringValue);
        if (ImGui::InputTextMultiline("Comment", TextBuf, sizeof(TextBuf), ImVec2(-1, 100)))
        {
            Node.StringValue = TextBuf;
            Blueprint->BumpEditorVersion();
            CommitBlueprintEdit(Blueprint);
        }
        float Size[2] = { Node.VectorValue.X, Node.VectorValue.Y };
        if (ImGui::DragFloat2("Group Size", Size, 1.0f, 80.0f, 4000.0f, "%.0f"))
        {
            Node.VectorValue = FVector(Size[0], Size[1], Node.VectorValue.Z);
            Blueprint->BumpEditorVersion();
            CommitBlueprintEdit(Blueprint);
        }
        ImGui::TextDisabled("그룹 박스를 끌면 안에 있는 노드도 같이 이동합니다.");
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
                    if (Pin.Kind == ELuaBlueprintPinKind::Input && Pin.Type == ELuaBlueprintPinType::String && Pin.
                        DisplayName.ToString() == "Text")
                    {
                        Pin.DefaultString = Node.StringValue;
                        break;
                    }
                }
            }
            Blueprint->BumpVersion();
            CommitBlueprintEdit(Blueprint);
        }
        break;
    }
    case ELuaBlueprintNodeType::LiteralBool:
        if (ImGui::Checkbox("Value", &Node.BoolValue))
        {
            Blueprint->BumpVersion();
            CommitBlueprintEdit(Blueprint);
        }
        break;
    case ELuaBlueprintNodeType::LiteralInt:
        if (ImGui::InputInt("Value", &Node.IntValue))
        {
            Blueprint->BumpVersion();
            CommitBlueprintEdit(Blueprint);
        }
        break;
    case ELuaBlueprintNodeType::LiteralFloat:
        if (ImGui::InputFloat("Value", &Node.FloatValue))
        {
            Blueprint->BumpVersion();
            CommitBlueprintEdit(Blueprint);
        }
        break;
    case ELuaBlueprintNodeType::LiteralVector:
    {
        float V[3] = { Node.VectorValue.X, Node.VectorValue.Y, Node.VectorValue.Z };
        if (ImGui::InputFloat3("Value", V))
        {
            Node.VectorValue = FVector(V[0], V[1], V[2]);
            Blueprint->BumpVersion();
            CommitBlueprintEdit(Blueprint);
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
        FString     Current = Node.NameValue.ToString();
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
                    CommitBlueprintEdit(Blueprint);
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
            CommitBlueprintEdit(Blueprint);
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
            CommitBlueprintEdit(Blueprint);
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
        ImGui::BulletText(
            "%s %s %s #%u",
            Pin.Kind == ELuaBlueprintPinKind::Input ? "In" : "Out",
            PinTypeLabel(Pin.Type),
            Pin.DisplayName.ToString().c_str(),
            Pin.PinId
        );
        ImGui::PopStyleColor();
    }
}

void FLuaBlueprintEditorWidget::RenderVariableEditor(
    ULuaBlueprintAsset*    Blueprint,
    FLuaBlueprintVariable& Variable,
    int32                  Index
    )
{
    char NameBuf[128];
    CopyToBuffer(NameBuf, sizeof(NameBuf), Variable.Name.ToString());
    if (ImGui::InputText("Name", NameBuf, sizeof(NameBuf), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        const FName OldName = Variable.Name;
        const FName NewName = MakeUniqueVariableNameForEditor(Blueprint, NameBuf[0] ? FName(NameBuf) : FName::None, Index);
        Variable.Name       = NewName;
        CopyToBuffer(NameBuf, sizeof(NameBuf), Variable.Name.ToString());
        // 이 변수를 참조하는 Get/Set 노드의 NameValue 도 함께 갱신 — AnimGraph state rename 패턴.
        RenameVariableCascade(Blueprint, OldName, NewName);
        Blueprint->RefreshAllNodePinTypes();
        Blueprint->RemoveInvalidLinks();
        Blueprint->BumpVersion();
        CommitBlueprintEdit(Blueprint);
    }

    Variable.Type = NormalizeVariablePinTypeForEditor(Variable.Type);
    int         TypeIndex    = VariablePinTypeToComboIndex(Variable.Type);
    const char* TypeLabels[] = { "Bool", "Int", "Float", "String", "Vector", "Object", "Array" };
    if (ImGui::Combo("Type", &TypeIndex, TypeLabels, IM_ARRAYSIZE(TypeLabels)))
    {
        Variable.Type = ComboIndexToVariablePinType(TypeIndex);
        Blueprint->RefreshAllNodePinTypes();
        Blueprint->RemoveInvalidLinks();
        Blueprint->BumpVersion();
        CommitBlueprintEdit(Blueprint);
    }

    switch (Variable.Type)
    {
    case ELuaBlueprintPinType::Bool:
        if (ImGui::Checkbox("Default", &Variable.BoolValue))
        {
            Blueprint->BumpVersion();
            CommitBlueprintEdit(Blueprint);
        }
        break;
    case ELuaBlueprintPinType::Int:
        if (ImGui::InputInt("Default", &Variable.IntValue))
        {
            Blueprint->BumpVersion();
            CommitBlueprintEdit(Blueprint);
        }
        break;
    case ELuaBlueprintPinType::Float:
        if (ImGui::InputFloat("Default", &Variable.FloatValue))
        {
            Blueprint->BumpVersion();
            CommitBlueprintEdit(Blueprint);
        }
        break;
    case ELuaBlueprintPinType::String:
    {
        char ValueBuf[512];
        CopyToBuffer(ValueBuf, sizeof(ValueBuf), Variable.StringValue);
        if (ImGui::InputText("Default", ValueBuf, sizeof(ValueBuf)))
        {
            Variable.StringValue = ValueBuf;
            Blueprint->BumpVersion();
            CommitBlueprintEdit(Blueprint);
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
            CommitBlueprintEdit(Blueprint);
        }
        break;
    }
    case ELuaBlueprintPinType::Object:
        if (ImGui::Checkbox("Strong Object Ref", &Variable.bStrongObject))
        {
            Blueprint->BumpVersion();
            CommitBlueprintEdit(Blueprint);
        }
        break;
    default:
        break;
    }

    // Get/Set 빠른 추가 버튼 — 실제 BP 의 ctrl/alt+drag 대안. PendingNewNodePosition 가 비어있으면
    // 캔버스 우상단 근처 기본 위치로.
    const ImVec2 SpawnPos = (PendingNewNodePosition.x != 0.0f || PendingNewNodePosition.y != 0.0f)
    ? PendingNewNodePosition : ImVec2(100.0f + Index * 30.0f, 100.0f + Index * 30.0f);

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
            const FName RemovedName = Vars[Index].Name;
            Vars.erase(Vars.begin() + Index);
            RemoveVariableCascade(Blueprint, RemovedName);
            Blueprint->RefreshAllNodePinTypes();
            Blueprint->RemoveInvalidLinks();
            Blueprint->BumpVersion();
            CommitBlueprintEdit(Blueprint);
        }
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
        case ELuaBlueprintDiagnosticSeverity::Error:
            Color = ImVec4(1.0f, 0.45f, 0.45f, 1.0f);
            break;
        case ELuaBlueprintDiagnosticSeverity::Warning:
            Color = ImVec4(0.95f, 0.85f, 0.35f, 1.0f);
            break;
        default:
            break;
        }
        ImGui::TextColored(
            Color,
            "[%s] Node %u: %s",
            SeverityLabel(Diagnostic.Severity),
            Diagnostic.NodeId,
            Diagnostic.Message.c_str()
        );
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

bool FLuaBlueprintEditorWidget::RenderInlinePinLiteral(
    ULuaBlueprintAsset* Blueprint,
    FLuaBlueprintNode& /*Node*/,
    FLuaBlueprintPin& Pin
    )
{
    // 데이터 input pin 만 inline editor 노출. Exec/Object/Any/Array 는 literal 편집 대상이 아니다.
    if (Pin.Kind != ELuaBlueprintPinKind::Input) return false;
    if (Pin.Type == ELuaBlueprintPinType::Exec || Pin.Type == ELuaBlueprintPinType::Object || Pin.Type ==
        ELuaBlueprintPinType::Any || Pin.Type == ELuaBlueprintPinType::Array) return false;

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
            bChanged          = true;
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
            bChanged          = true;
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
        CommitBlueprintEdit(Blueprint);
    }
    return bChanged;
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
            FScopedNodeEditorCurrent Scope(NodeEditorContext);
            ed::SetNodePosition(ToNodeId(NewNode->NodeId), PendingNewNodePosition);
        }
        CommitBlueprintEdit(Blueprint);
    }
    if (bDisabled) ImGui::EndDisabled();
    return bClicked;
}

bool FLuaBlueprintEditorWidget::AddVariableMenuItem(
    ULuaBlueprintAsset*  Blueprint,
    ELuaBlueprintPinType Type,
    const char*          Label
    )
{
    if (!ImGui::MenuItem(Label)) return false;

    char NameBuf[48];
    std::snprintf(NameBuf, sizeof(NameBuf), "Var%u", static_cast<uint32>(Blueprint->GetVariables().size()));
    Blueprint->AddVariable(FName(NameBuf), Type);
    CommitBlueprintEdit(Blueprint);
    return true;
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
        for (int32 i = 0; i <= static_cast<int32>(ELuaBlueprintNodeType::HasEventBinding); ++i)
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
            AddItem(ELuaBlueprintNodeType::ForEachArray);
            AddItem(ELuaBlueprintNodeType::Delay);
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
            AddItem(ELuaBlueprintNodeType::CustomEvent);
            AddItem(ELuaBlueprintNodeType::CallCustomEvent);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Delegates"))
        {
            AddItem(ELuaBlueprintNodeType::BindEvent);
            AddItem(ELuaBlueprintNodeType::UnbindEvent);
            AddItem(ELuaBlueprintNodeType::HasEventBinding);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Conversions"))
        {
            AddItem(ELuaBlueprintNodeType::ToBool);
            AddItem(ELuaBlueprintNodeType::ToInt);
            AddItem(ELuaBlueprintNodeType::ToFloat);
            AddItem(ELuaBlueprintNodeType::ToString);
            AddItem(ELuaBlueprintNodeType::ToVector);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Graph Utility"))
        {
            AddItem(ELuaBlueprintNodeType::Reroute);
            AddItem(ELuaBlueprintNodeType::Comment);
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

void FLuaBlueprintEditorWidget::RenderPinSpawnMenu(ULuaBlueprintAsset* Blueprint)
{
    const FLuaBlueprintPin* DragPin = Blueprint ? Blueprint->FindPin(PendingPinSpawnPinId) : nullptr;
    if (!DragPin)
    {
        ImGui::TextDisabled("No pin context.");
        return;
    }

    ImGui::TextDisabled(
        DragPin->Kind == ELuaBlueprintPinKind::Output
        ? "Nodes accepting %s input"
        : "Nodes producing %s output",
        PinTypeLabel(DragPin->Type)
    );
    ImGui::Separator();
    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputTextWithHint("##PinSpawnSearch", "search compatible nodes...", PinSpawnSearchBuf, sizeof(PinSpawnSearchBuf));
    ImGui::Separator();

    int32 NumShown = 0;
    for (int32 i = 0; i <= static_cast<int32>(ELuaBlueprintNodeType::HasEventBinding); ++i)
    {
        const ELuaBlueprintNodeType Type = static_cast<ELuaBlueprintNodeType>(i);
        if (!ContainsCaseInsensitive(NodeTypeLabel(Type), PinSpawnSearchBuf)) continue;

        const size_t BeforeNodes = Blueprint ? Blueprint->GetNodes().size() : 0;
        if (AddContextNodeMenuItem(Blueprint, Type))
        {
            PendingPinSpawnPinId = 0;
            ImGui::CloseCurrentPopup();
            return;
        }
        const size_t AfterNodes = Blueprint ? Blueprint->GetNodes().size() : 0;
        if (AfterNodes == BeforeNodes && NodeTypeCanConnectToPendingPin(Type))
        {
            ++NumShown;
        }
    }

    if (NumShown == 0)
    {
        ImGui::TextDisabled("No compatible nodes.");
    }
}

bool FLuaBlueprintEditorWidget::AddContextNodeMenuItem(ULuaBlueprintAsset* Blueprint, ELuaBlueprintNodeType Type)
{
    if (!Blueprint || PendingPinSpawnPinId == 0) return false;
    const FLuaBlueprintPin* DragPin = Blueprint->FindPin(PendingPinSpawnPinId);
    if (!DragPin) return false;
    if (IsEventNode(Type) && HasNodeOfType(Blueprint, Type)) return false;
    const FLuaBlueprintPin DragPinCopy = *DragPin;
    if (!NodeTypeCanConnectToPendingPin(Type)) return false;

    ImGui::PushStyleColor(ImGuiCol_Text, NodeHeaderColor(Type));
    const bool bClicked = ImGui::MenuItem(NodeTypeLabel(Type));
    ImGui::PopStyleColor();
    if (!bClicked) return false;

    FLuaBlueprintNode* NewNode = Blueprint->AddNodeOfType(Type, PendingPinSpawnPos.x, PendingPinSpawnPos.y);
    if (!NewNode) return false;
    Blueprint->RefreshNodePinTypes(*NewNode);

    if (NodeEditorContext)
    {
        FScopedNodeEditorCurrent Scope(NodeEditorContext);
        ed::SetNodePosition(ToNodeId(NewNode->NodeId), PendingPinSpawnPos);
    }

    FLuaBlueprintPin* OtherPin = FindFirstCompatiblePinOnNode(Blueprint, *NewNode, DragPinCopy);
    if (OtherPin)
    {
        uint32 FromPinId = 0;
        uint32 ToPinIdValue = 0;
        if (Blueprint->CanLinkPins(DragPinCopy.PinId, OtherPin->PinId, &FromPinId, &ToPinIdValue))
        {
            Blueprint->AddLink(FromPinId, ToPinIdValue);
        }
    }
    CommitBlueprintEdit(Blueprint);
    return true;
}

bool FLuaBlueprintEditorWidget::NodeTypeCanConnectToPendingPin(ELuaBlueprintNodeType Type) const
{
    if (PendingPinSpawnPinId == 0) return true;
    const ULuaBlueprintAsset* CurrentBlueprint = GetBlueprint();
    if (!CurrentBlueprint) return false;
    if (IsEventNode(Type) && HasNodeOfType(CurrentBlueprint, Type)) return false;
    const FLuaBlueprintPin* DragPin = CurrentBlueprint->FindPin(PendingPinSpawnPinId);
    if (!DragPin) return false;

    ULuaBlueprintAsset Scratch;
    FLuaBlueprintNode* Probe = Scratch.AddNodeOfType(Type, 0.0f, 0.0f);
    if (!Probe) return false;

    for (const FLuaBlueprintPin& Candidate : Probe->Pins)
    {
        if (Candidate.Kind == DragPin->Kind) continue;

        // Output pin → blank: show nodes whose input can accept that output, including
        // implicit conversion.
        if (DragPin->Kind == ELuaBlueprintPinKind::Output && Candidate.Kind == ELuaBlueprintPinKind::Input)
        {
            if (ULuaBlueprintAsset::ArePinTypesCompatibleForLink(DragPin->Type, Candidate.Type))
            {
                return true;
            }
            continue;
        }

        // Input pin → blank: the produced output should be the same visible type as the
        // dragged input. Any is allowed only for nodes that resolve their pin type on link
        // such as Reroute/GetVariable before a variable is chosen.
        if (DragPin->Kind == ELuaBlueprintPinKind::Input && Candidate.Kind == ELuaBlueprintPinKind::Output)
        {
            if (DragPin->Type == ELuaBlueprintPinType::Any || Candidate.Type == ELuaBlueprintPinType::Any || Candidate.Type == DragPin->Type)
            {
                return true;
            }
        }
    }
    return false;
}

FLuaBlueprintPin* FLuaBlueprintEditorWidget::FindFirstCompatiblePinOnNode(
    ULuaBlueprintAsset* Blueprint,
    FLuaBlueprintNode&  Node,
    const FLuaBlueprintPin& DragPin
    ) const
{
    if (!Blueprint) return nullptr;
    for (FLuaBlueprintPin& Candidate : Node.Pins)
    {
        if (Candidate.Kind == DragPin.Kind) continue;
        uint32 FromPinId = 0;
        uint32 ToPinId = 0;
        if (Blueprint->CanLinkPins(DragPin.PinId, Candidate.PinId, &FromPinId, &ToPinId))
        {
            return &Candidate;
        }
    }
    return nullptr;
}

void FLuaBlueprintEditorWidget::HandleVariableDropOnCanvas()
{
    // 캔버스 child window 의 전체 영역을 custom drop target 으로 사용.
    // ScreenToCanvas 변환은 다음 프레임의 ed::Begin 안에서 안전하게 처리 (bPendingVariableDrop 플래그).
    const ImGuiID ChildId = ImGui::GetCurrentWindow()->ID;
    const ImRect  ChildRect(
        ImGui::GetWindowPos(),
        ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x, ImGui::GetWindowPos().y + ImGui::GetWindowSize().y)
    );
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

void FLuaBlueprintEditorWidget::RenameVariableCascade(
    ULuaBlueprintAsset* Blueprint,
    const FName&        OldName,
    const FName&        NewName
    )
{
    if (OldName == NewName) return;
    for (FLuaBlueprintNode& Node : Blueprint->GetMutableNodes())
    {
        if (Node.Type != ELuaBlueprintNodeType::GetVariable && Node.Type != ELuaBlueprintNodeType::SetVariable) {continue
        ;}
        if (Node.NameValue == OldName) Node.NameValue = NewName;
    }
}

void FLuaBlueprintEditorWidget::SpawnVariableNode(
    ULuaBlueprintAsset*   Blueprint,
    ELuaBlueprintNodeType Type,
    const FName&          VariableName,
    const ImVec2&         Position
    )
{
    if (Type != ELuaBlueprintNodeType::GetVariable && Type != ELuaBlueprintNodeType::SetVariable) return;

    FLuaBlueprintNode* NewNode = Blueprint->AddNodeOfType(Type, Position.x, Position.y);
    if (!NewNode) return;

    NewNode->NameValue = VariableName;
    Blueprint->RefreshNodePinTypes(*NewNode);
    if (NodeEditorContext)
    {
        FScopedNodeEditorCurrent Scope(NodeEditorContext);
        ed::SetNodePosition(ToNodeId(NewNode->NodeId), Position);
    }
    Blueprint->BumpVersion();
    CommitBlueprintEdit(Blueprint);
}

void FLuaBlueprintEditorWidget::CaptureInitialUndoSnapshot(ULuaBlueprintAsset* Blueprint)
{
    UndoStack.clear();
    RedoStack.clear();
    ClipboardNodes.clear();
    ClipboardLinks.clear();
    const TArray<uint8> Snapshot = CaptureLuaBlueprintSnapshot(Blueprint);
    if (!Snapshot.empty())
    {
        UndoStack.push_back(Snapshot);
    }
}

void FLuaBlueprintEditorWidget::CommitBlueprintEdit(ULuaBlueprintAsset* Blueprint)
{
    if (!Blueprint || bRestoringSnapshot) return;
    const TArray<uint8> Snapshot = CaptureLuaBlueprintSnapshot(Blueprint);
    if (!Snapshot.empty() && (UndoStack.empty() || UndoStack.back() != Snapshot))
    {
        UndoStack.push_back(Snapshot);
        if (UndoStack.size() > 128)
        {
            UndoStack.erase(UndoStack.begin());
        }
    }
    RedoStack.clear();
    MarkDirty();
}

void FLuaBlueprintEditorWidget::UndoBlueprintEdit(ULuaBlueprintAsset* Blueprint)
{
    if (!Blueprint || UndoStack.size() <= 1) return;
    RedoStack.push_back(UndoStack.back());
    UndoStack.pop_back();
    RestoreBlueprintSnapshot(Blueprint, UndoStack.back());
}

void FLuaBlueprintEditorWidget::RedoBlueprintEdit(ULuaBlueprintAsset* Blueprint)
{
    if (!Blueprint || RedoStack.empty()) return;
    const TArray<uint8> Snapshot = RedoStack.back();
    RedoStack.pop_back();
    UndoStack.push_back(Snapshot);
    RestoreBlueprintSnapshot(Blueprint, Snapshot);
}

bool FLuaBlueprintEditorWidget::RestoreBlueprintSnapshot(ULuaBlueprintAsset* Blueprint, const TArray<uint8>& Snapshot)
{
    if (!Blueprint || Snapshot.empty()) return false;
    bRestoringSnapshot = true;
    FMemoryArchive Loader(Snapshot, false);
    Blueprint->Serialize(Loader);
    Blueprint->Compile();
    bRestoringSnapshot = false;
    bPositionsPushed = false;
    MarkDirty();
    return true;
}

bool FLuaBlueprintEditorWidget::GatherSelectedNodes(
    ULuaBlueprintAsset* Blueprint,
    TArray<FLuaBlueprintNode>& OutNodes,
    TArray<FLuaBlueprintLink>& OutLinks
    ) const
{
    OutNodes.clear();
    OutLinks.clear();
    if (!Blueprint || !NodeEditorContext) return false;

    FScopedNodeEditorCurrent Scope(NodeEditorContext);

    ed::NodeId Selected[512];
    const int Count = ed::GetSelectedNodes(Selected, 512);
    if (Count <= 0) return false;

    std::unordered_set<uint32> SelectedIds;
    SelectedIds.reserve(static_cast<size_t>(Count));
    for (int i = 0; i < Count; ++i)
    {
        SelectedIds.insert(NodeIdToU32(Selected[i]));
    }

    for (const FLuaBlueprintNode& Node : Blueprint->GetNodes())
    {
        if (SelectedIds.count(Node.NodeId))
        {
            FLuaBlueprintNode SnapshotNode = Node;
            const ImVec2 EditorPos = ed::GetNodePosition(ToNodeId(Node.NodeId));
            SnapshotNode.PosX = EditorPos.x;
            SnapshotNode.PosY = EditorPos.y;
            OutNodes.push_back(SnapshotNode);
        }
    }

    for (const FLuaBlueprintLink& Link : Blueprint->GetLinks())
    {
        const FLuaBlueprintPin* From = Blueprint->FindPin(Link.FromPinId);
        const FLuaBlueprintPin* To   = Blueprint->FindPin(Link.ToPinId);
        if (From && To && SelectedIds.count(From->OwningNodeId) && SelectedIds.count(To->OwningNodeId))
        {
            OutLinks.push_back(Link);
        }
    }

    return !OutNodes.empty();
}

bool FLuaBlueprintEditorWidget::CloneNodeFragment(
    ULuaBlueprintAsset* Blueprint,
    const TArray<FLuaBlueprintNode>& SourceNodes,
    const TArray<FLuaBlueprintLink>& SourceLinks,
    const ImVec2& TargetAnchor,
    TArray<uint32>* OutNewNodeIds,
    const ImVec2* SourceAnchorOverride
    )
{
    if (!Blueprint || SourceNodes.empty()) return false;

    const ImVec2 SourceAnchor = SourceAnchorOverride ? *SourceAnchorOverride : ComputeNodeFragmentMin(SourceNodes);
    const ImVec2 Delta(TargetAnchor.x - SourceAnchor.x, TargetAnchor.y - SourceAnchor.y);

    std::unordered_map<uint32, uint32> PinIdMap;
    TArray<uint32> NewNodeIds;
    bool bChanged = false;

    // ed::SetNodePosition 호출이 필요하므로 ed 컨텍스트를 활성화. CloneNodeFragment 가
    // RenderGraph 의 ed::Begin/End 안에서 호출되는 경우(ProcessQueuedNodeEditorCommands)는
    // 이미 current 이라 no-op. 바깥(외부 API)에서 호출돼도 안전하게 동작.
    FScopedNodeEditorCurrent EdScope(NodeEditorContext);

    for (const FLuaBlueprintNode& SrcNode : SourceNodes)
    {
        if (IsEventNode(SrcNode.Type) && HasNodeOfType(Blueprint, SrcNode.Type))
        {
            continue;
        }

        const float NewX = SrcNode.PosX + Delta.x;
        const float NewY = SrcNode.PosY + Delta.y;
        FLuaBlueprintNode* NewNode = Blueprint->AddNodeOfType(SrcNode.Type, NewX, NewY);
        if (!NewNode) continue;

        NewNode->DisplayName = SrcNode.DisplayName;
        NewNode->NameValue   = SrcNode.NameValue;
        NewNode->StringValue = SrcNode.StringValue;
        NewNode->BoolValue   = SrcNode.BoolValue;
        NewNode->IntValue    = SrcNode.IntValue;
        NewNode->FloatValue  = SrcNode.FloatValue;
        NewNode->VectorValue = SrcNode.VectorValue;

        const size_t PinCount = std::min(NewNode->Pins.size(), SrcNode.Pins.size());
        for (size_t PinIndex = 0; PinIndex < PinCount; ++PinIndex)
        {
            const FLuaBlueprintPin& SrcPin = SrcNode.Pins[PinIndex];
            FLuaBlueprintPin& DstPin = NewNode->Pins[PinIndex];
            DstPin.Type          = SrcPin.Type;
            DstPin.DisplayName   = SrcPin.DisplayName;
            DstPin.DefaultBool   = SrcPin.DefaultBool;
            DstPin.DefaultInt    = SrcPin.DefaultInt;
            DstPin.DefaultFloat  = SrcPin.DefaultFloat;
            DstPin.DefaultString = SrcPin.DefaultString;
            DstPin.DefaultVector = SrcPin.DefaultVector;
            PinIdMap[SrcPin.PinId] = DstPin.PinId;
        }
        // 데이터모델 PosX/PosY 만 세팅하면 RenderGraph 끝의 sync loop 가 ed 의 (0,0) 으로
        // 덮어쓴다 (새 노드는 아직 ed 에 push 안 된 상태). 즉시 ed::SetNodePosition 으로 push.
        if (NodeEditorContext)
        {
            ed::SetNodePosition(ToNodeId(NewNode->NodeId), ImVec2(NewX, NewY));
        }
        NewNodeIds.push_back(NewNode->NodeId);
        bChanged = true;
    }

    for (const FLuaBlueprintLink& SrcLink : SourceLinks)
    {
        auto FromIt = PinIdMap.find(SrcLink.FromPinId);
        auto ToIt   = PinIdMap.find(SrcLink.ToPinId);
        if (FromIt != PinIdMap.end() && ToIt != PinIdMap.end())
        {
            uint32 FromPinId = 0;
            uint32 ToPinId   = 0;
            if (Blueprint->CanLinkPins(FromIt->second, ToIt->second, &FromPinId, &ToPinId))
            {
                Blueprint->AddLink(FromPinId, ToPinId);
            }
        }
    }

    Blueprint->RefreshAllNodePinTypes();
    if (OutNewNodeIds)
    {
        *OutNewNodeIds = NewNodeIds;
    }
    return bChanged;
}

void FLuaBlueprintEditorWidget::SelectOnlyNodes(const TArray<uint32>& NodeIds)
{
    if (!NodeEditorContext) return;

    FScopedNodeEditorCurrent Scope(NodeEditorContext);
    ed::ClearSelection();

    bool bAppend = false;
    for (uint32 NodeId : NodeIds)
    {
        if (NodeId == 0) continue;
        ed::SelectNode(ToNodeId(NodeId), bAppend);
        bAppend = true;
    }
}



void FLuaBlueprintEditorWidget::CopySelectedNodes(ULuaBlueprintAsset* Blueprint)
{
    GatherSelectedNodes(Blueprint, ClipboardNodes, ClipboardLinks);
}

void FLuaBlueprintEditorWidget::PasteCopiedNodes(ULuaBlueprintAsset* Blueprint, const ImVec2* OverrideAnchor)
{
    if (!Blueprint || ClipboardNodes.empty()) return;

    FScopedNodeEditorCurrent Scope(NodeEditorContext);

    FLuaBlueprintNodeFragmentBounds ReferenceBounds;
    ImVec2 Anchor = OverrideAnchor ? *OverrideAnchor : ImVec2(0.0f, 0.0f);
    const ImVec2* SourceAnchorOverride = nullptr;

    if (!OverrideAnchor)
    {
        ReferenceBounds = ComputeNodeFragmentPasteReferenceBounds(ClipboardNodes, NodeEditorContext != nullptr);
        if (ReferenceBounds.bValid)
        {
            constexpr float PasteGap = 10.0f;
            Anchor = ImVec2(ReferenceBounds.Max.x + PasteGap, ReferenceBounds.Min.y + PasteGap);
            SourceAnchorOverride = &ReferenceBounds.Min;
        }
    }

    if (Anchor.x == 0.0f && Anchor.y == 0.0f)
    {
        const ImVec2 SourceAnchor = ComputeNodeFragmentMin(ClipboardNodes);
        Anchor = ImVec2(SourceAnchor.x + 10.0f, SourceAnchor.y + 10.0f);
    }

    TArray<uint32> NewNodeIds;
    if (CloneNodeFragment(Blueprint, ClipboardNodes, ClipboardLinks, Anchor, &NewNodeIds, SourceAnchorOverride))
    {
        SelectOnlyNodes(NewNodeIds);
        bPositionsPushed = false;
        CommitBlueprintEdit(Blueprint);
    }
}

void FLuaBlueprintEditorWidget::DeleteSelectedNodes(ULuaBlueprintAsset* Blueprint)
{
    if (!Blueprint || !NodeEditorContext) return;
    FScopedNodeEditorCurrent Scope(NodeEditorContext);
    ed::NodeId Selected[512];
    const int Count = ed::GetSelectedNodes(Selected, 512);
    if (Count <= 0) return;

    TArray<uint32> RootNodeIds;
    RootNodeIds.reserve(static_cast<size_t>(Count));
    for (int i = 0; i < Count; ++i)
    {
        RootNodeIds.push_back(NodeIdToU32(Selected[i]));
    }

    if (DeleteNodesIncludingContainedGroups(Blueprint, RootNodeIds))
    {
        CommitBlueprintEdit(Blueprint);
    }
}

bool FLuaBlueprintEditorWidget::DeleteNodesIncludingContainedGroups(
    ULuaBlueprintAsset* Blueprint,
    const TArray<uint32>& RootNodeIds
    )
{
    if (!Blueprint || RootNodeIds.empty()) return false;

    std::unordered_set<uint32> NodeIdsToDelete;
    for (uint32 NodeId : RootNodeIds)
    {
        if (NodeId != 0)
        {
            NodeIdsToDelete.insert(NodeId);
        }
    }

    // Comment/group membership is geometric in imgui-node-editor. When a comment is removed,
    // recursively collect every node fully contained by that comment rectangle, including nested comments.
    bool bExpanded = true;
    while (bExpanded)
    {
        bExpanded = false;
        TArray<uint32> SnapshotIds;
        SnapshotIds.reserve(NodeIdsToDelete.size());
        for (uint32 NodeId : NodeIdsToDelete)
        {
            SnapshotIds.push_back(NodeId);
        }

        for (uint32 NodeId : SnapshotIds)
        {
            const FLuaBlueprintNode* CommentNode = Blueprint->FindNode(NodeId);
            if (!CommentNode || CommentNode->Type != ELuaBlueprintNodeType::Comment) continue;

            const FLuaBlueprintNodeFragmentBounds CommentBounds = ComputeLiveNodeBounds(*CommentNode, NodeEditorContext != nullptr);
            if (!CommentBounds.bValid) continue;

            for (const FLuaBlueprintNode& Candidate : Blueprint->GetNodes())
            {
                if (Candidate.NodeId == CommentNode->NodeId) continue;
                if (NodeIdsToDelete.count(Candidate.NodeId)) continue;

                const FLuaBlueprintNodeFragmentBounds CandidateBounds = ComputeLiveNodeBounds(Candidate, NodeEditorContext != nullptr);
                if (IsBoundsFullyInside(CandidateBounds, CommentBounds))
                {
                    NodeIdsToDelete.insert(Candidate.NodeId);
                    bExpanded = true;
                }
            }
        }
    }

    bool bChanged = false;
    for (uint32 NodeId : NodeIdsToDelete)
    {
        bChanged = Blueprint->RemoveNode(NodeId) || bChanged;
    }

    if (bChanged)
    {
        Blueprint->RefreshAllNodePinTypes();
        bPositionsPushed = false;
    }
    return bChanged;
}

void FLuaBlueprintEditorWidget::DuplicateSelectedNodes(ULuaBlueprintAsset* Blueprint)
{
    FScopedNodeEditorCurrent Scope(NodeEditorContext);

    TArray<FLuaBlueprintNode> SelectedNodes;
    TArray<FLuaBlueprintLink> SelectedLinks;
    if (!GatherSelectedNodes(Blueprint, SelectedNodes, SelectedLinks)) return;

    const FLuaBlueprintNodeFragmentBounds ReferenceBounds = ComputeNodeFragmentPasteReferenceBounds(SelectedNodes, NodeEditorContext != nullptr);
    const ImVec2 SourceAnchor = ReferenceBounds.bValid ? ReferenceBounds.Min : ComputeNodeFragmentMin(SelectedNodes);
    constexpr float PasteGap = 10.0f;
    const ImVec2 TargetAnchor = ReferenceBounds.bValid
        ? ImVec2(ReferenceBounds.Max.x + PasteGap, ReferenceBounds.Min.y + PasteGap)
        : ImVec2(SourceAnchor.x + PasteGap, SourceAnchor.y + PasteGap);

    TArray<uint32> NewNodeIds;
    if (CloneNodeFragment(Blueprint, SelectedNodes, SelectedLinks, TargetAnchor, &NewNodeIds, &SourceAnchor))
    {
        SelectOnlyNodes(NewNodeIds);
        bPositionsPushed = false;
        CommitBlueprintEdit(Blueprint);
    }
}

void FLuaBlueprintEditorWidget::GroupSelectedNodesAsComment(ULuaBlueprintAsset* Blueprint)
{
    if (!Blueprint || !NodeEditorContext) return;
    FScopedNodeEditorCurrent Scope(NodeEditorContext);

    ed::NodeId Selected[512];
    const int Count = ed::GetSelectedNodes(Selected, 512);
    if (Count <= 0) return;

    // ed 가 가진 위치/크기로 bounding box 계산 (데이터모델 PosX/PosY 는 한 프레임 뒤처질 수 있음).
    float MinX = FLT_MAX, MinY = FLT_MAX, MaxX = -FLT_MAX, MaxY = -FLT_MAX;
    bool bAny = false;
    for (int i = 0; i < Count; ++i)
    {
        const uint32 NodeIdU = NodeIdToU32(Selected[i]);
        const FLuaBlueprintNode* SrcNode = Blueprint->FindNode(NodeIdU);
        if (!SrcNode) continue;

        // Existing comments/groups are now included in the bounds. This lets a new group wrap
        // nested groups instead of ignoring them and creating a too-small comment box.
        const FLuaBlueprintNodeFragmentBounds NodeBounds = ComputeLiveNodeBounds(*SrcNode, true);
        if (!NodeBounds.bValid) continue;

        bAny = true;
        MinX = std::min(MinX, NodeBounds.Min.x);
        MinY = std::min(MinY, NodeBounds.Min.y);
        MaxX = std::max(MaxX, NodeBounds.Max.x);
        MaxY = std::max(MaxY, NodeBounds.Max.y);
    }
    if (!bAny) return;

    const float Pad      = 24.0f;
    const float Header   = 28.0f; // 위쪽에 코멘트 타이틀이 들어갈 여유
    const ImVec2 GroupPos(MinX - Pad, MinY - Pad - Header);
    const ImVec2 GroupSize((MaxX - MinX) + Pad * 2.0f, (MaxY - MinY) + Pad * 2.0f + Header);

    FLuaBlueprintNode* CommentNode = Blueprint->AddNodeOfType(
        ELuaBlueprintNodeType::Comment, GroupPos.x, GroupPos.y);
    if (!CommentNode) return;

    CommentNode->StringValue = "Group";
    CommentNode->VectorValue = FVector(GroupSize.x, GroupSize.y, 0.0f);

    // ed 에 즉시 push — 안 그러면 다음 sync 루프가 (0,0) 로 덮어쓴다 (CloneNodeFragment 와 같은 이유).
    ed::SetNodePosition(ToNodeId(CommentNode->NodeId), GroupPos);

    CommitBlueprintEdit(Blueprint);
}

void FLuaBlueprintEditorWidget::ProcessQueuedNodeEditorCommands(ULuaBlueprintAsset* Blueprint)
{
    if (!Blueprint || !NodeEditorContext)
    {
        bQueuedCopySelected = false;
        bQueuedPasteNodes = false;
        bQueuedDuplicateSelected = false;
        bQueuedDeleteSelected = false;
        bQueuedGroupSelected = false;
        return;
    }

    // 이 함수는 RenderGraph() 내부, ed::Begin() 이후에 호출된다.
    // 그래도 메뉴/단축키 경로가 나중에 바뀌어도 안전하도록 current editor를 다시 보장한다.
    FScopedNodeEditorCurrent Scope(NodeEditorContext);

    if (bQueuedCopySelected)
    {
        CopySelectedNodes(Blueprint);
    }
    if (bQueuedPasteNodes)
    {
        PasteCopiedNodes(Blueprint);
    }
    if (bQueuedDuplicateSelected)
    {
        DuplicateSelectedNodes(Blueprint);
    }
    if (bQueuedDeleteSelected)
    {
        DeleteSelectedNodes(Blueprint);
    }
    if (bQueuedGroupSelected)
    {
        GroupSelectedNodesAsComment(Blueprint);
    }

    bQueuedCopySelected = false;
    bQueuedPasteNodes = false;
    bQueuedDuplicateSelected = false;
    bQueuedDeleteSelected = false;
    bQueuedGroupSelected = false;
}

void FLuaBlueprintEditorWidget::RemoveVariableCascade(ULuaBlueprintAsset* Blueprint, const FName& VariableName)
{
    if (!Blueprint || VariableName == FName::None) return;

    TArray<uint32> NodesToRemove;
    for (const FLuaBlueprintNode& Node : Blueprint->GetNodes())
    {
        if ((Node.Type == ELuaBlueprintNodeType::GetVariable || Node.Type == ELuaBlueprintNodeType::SetVariable) &&
            Node.NameValue == VariableName)
        {
            NodesToRemove.push_back(Node.NodeId);
        }
    }

    for (uint32 NodeId : NodesToRemove)
    {
        Blueprint->RemoveNode(NodeId);
    }
}

void FLuaBlueprintEditorWidget::RenderInputPinConnectionStatus(ULuaBlueprintAsset* Blueprint, const FLuaBlueprintPin& Pin)
{
    if (!Blueprint || Pin.Kind != ELuaBlueprintPinKind::Input) return;
    const FLuaBlueprintLink* Link = Blueprint->FindLinkToInput(Pin.PinId);
    if (!Link) return;

    const FLuaBlueprintPin* SourcePin = Blueprint->FindPin(Link->FromPinId);
    if (!SourcePin) return;
    if (SourcePin->Type == ELuaBlueprintPinType::Exec || Pin.Type == ELuaBlueprintPinType::Exec) return;

    if (SourcePin->Type != Pin.Type && ULuaBlueprintAsset::ArePinTypesCompatibleForLink(SourcePin->Type, Pin.Type))
    {
        ImGui::TextDisabled("auto %s -> %s", PinTypeLabel(SourcePin->Type), PinTypeLabel(Pin.Type));
    }
    else
    {
        ImGui::TextDisabled("linked");
    }
}
