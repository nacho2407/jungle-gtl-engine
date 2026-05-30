#include "Editor/UI/Util/InlinePropertyRenderer.h"

#include "Core/Property/NumericProperty.h"
#include "Core/Types/PropertyTypes.h"
#include "ImGui/imgui.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Object/FName.h"
#include "Object/Object.h"
#include "Object/Reflection/UStruct.h"

#include <algorithm>
#include <cfloat>
#include <cstring>

namespace
{
	const char* GetDisplayName(const FPropertyValue& Prop)
	{
		const char* DisplayName = Prop.GetDisplayName();
		return DisplayName && *DisplayName ? DisplayName : Prop.GetName();
	}

	bool RenderValue(FPropertyValue& Prop)
	{
		switch (Prop.GetType())
		{
		case EPropertyType::Bool:
		{
			bool* Value = static_cast<bool*>(Prop.GetValuePtr());
			return Value && ImGui::Checkbox("##Value", Value);
		}
		case EPropertyType::ByteBool:
		{
			uint8* Value = static_cast<uint8*>(Prop.GetValuePtr());
			if (!Value)
			{
				return false;
			}

			bool bValue = (*Value != 0);
			if (ImGui::Checkbox("##Value", &bValue))
			{
				*Value = bValue ? 1 : 0;
				return true;
			}
			return false;
		}
		case EPropertyType::Int:
		{
			int32* Value = static_cast<int32*>(Prop.GetValuePtr());
			if (!Value)
			{
				return false;
			}

			const FNumericProperty* NumericProperty = Prop.Property ? Prop.Property->AsNumericProperty() : nullptr;
			const float Min = NumericProperty ? NumericProperty->GetMin() : Prop.GetMin();
			const float Max = NumericProperty ? NumericProperty->GetMax() : Prop.GetMax();
			const float Speed = NumericProperty ? NumericProperty->GetSpeed() : Prop.GetSpeed();
			return (Min != 0.0f || Max != 0.0f)
				? ImGui::DragInt("##Value", Value, Speed, static_cast<int32>(Min), static_cast<int32>(Max))
				: ImGui::DragInt("##Value", Value, Speed);
		}
		case EPropertyType::Float:
		{
			float* Value = static_cast<float*>(Prop.GetValuePtr());
			if (!Value)
			{
				return false;
			}

			const FNumericProperty* NumericProperty = Prop.Property ? Prop.Property->AsNumericProperty() : nullptr;
			const float Min = NumericProperty ? NumericProperty->GetMin() : Prop.GetMin();
			const float Max = NumericProperty ? NumericProperty->GetMax() : Prop.GetMax();
			const float Speed = NumericProperty ? NumericProperty->GetSpeed() : Prop.GetSpeed();
			return (Min != 0.0f || Max != 0.0f)
				? ImGui::DragFloat("##Value", Value, Speed, Min, Max, "%.4f")
				: ImGui::DragFloat("##Value", Value, Speed);
		}
		case EPropertyType::Vec3:
		{
			float* Value = static_cast<float*>(Prop.GetValuePtr());
			return Value && ImGui::DragFloat3("##Value", Value, Prop.GetSpeed());
		}
		case EPropertyType::Vec4:
		{
			float* Value = static_cast<float*>(Prop.GetValuePtr());
			return Value && ImGui::DragFloat4("##Value", Value, Prop.GetSpeed());
		}
		case EPropertyType::Rotator:
		{
			FRotator* Rotator = static_cast<FRotator*>(Prop.GetValuePtr());
			if (!Rotator)
			{
				return false;
			}

			float Rotation[3] = { Rotator->Roll, Rotator->Pitch, Rotator->Yaw };
			if (ImGui::DragFloat3("##Value", Rotation, Prop.GetSpeed()))
			{
				Rotator->Roll = Rotation[0];
				Rotator->Pitch = Rotation[1];
				Rotator->Yaw = Rotation[2];
				return true;
			}
			return false;
		}
		case EPropertyType::String:
		{
			FString* Value = static_cast<FString*>(Prop.GetValuePtr());
			if (!Value)
			{
				return false;
			}

			char Buffer[256];
			strncpy_s(Buffer, sizeof(Buffer), Value->c_str(), _TRUNCATE);
			if (ImGui::InputText("##Value", Buffer, sizeof(Buffer)))
			{
				*Value = Buffer;
				return true;
			}
			return false;
		}
		case EPropertyType::Name:
		{
			FName* Value = static_cast<FName*>(Prop.GetValuePtr());
			if (!Value)
			{
				return false;
			}

			FString Current = Value->ToString();
			char Buffer[256];
			strncpy_s(Buffer, sizeof(Buffer), Current.c_str(), _TRUNCATE);
			if (ImGui::InputText("##Value", Buffer, sizeof(Buffer)))
			{
				*Value = FName(FString(Buffer));
				return true;
			}
			return false;
		}
		default:
			ImGui::TextDisabled("(unsupported type)");
			return false;
		}
	}
}

namespace FInlinePropertyRenderer
{
	bool RenderStructProperties(UStruct* StructType, void* StructPtr, UObject* Owner, const char* TableId)
	{
		if (!StructType || !StructPtr)
		{
			ImGui::TextDisabled("(no editable properties)");
			return false;
		}

		TArray<const FProperty*> Properties;
		StructType->GetPropertyRefs(Properties);
		if (Properties.empty())
		{
			ImGui::TextDisabled("(no editable properties)");
			return false;
		}

		bool bAnyChanged = false;
		const char* EffectiveTableId = TableId ? TableId : "##InlineStructProperties";
		if (ImGui::BeginTable(EffectiveTableId, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
		{
			ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, 150.0f);
			ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);

			for (const FProperty* Property : Properties)
			{
				if (!Property || !Property->GetValuePtrFor(StructPtr))
				{
					continue;
				}

				FPropertyValue Prop = Property->ToValue(StructPtr, Owner);
				const bool bReadOnly = (Property->Flags & PF_ReadOnly) != 0;

				ImGui::PushID(Prop.GetName());
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(GetDisplayName(Prop));

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);

				if (bReadOnly)
				{
					ImGui::BeginDisabled();
				}

				const bool bChanged = RenderValue(Prop);

				if (bReadOnly)
				{
					ImGui::EndDisabled();
				}

				bAnyChanged |= !bReadOnly && bChanged;
				ImGui::PopID();
			}

			ImGui::EndTable();
		}

		return bAnyChanged;
	}
}
