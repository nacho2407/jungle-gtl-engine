#include "Editor/UI/Util/InlinePropertyRenderer.h"

#include "Core/Property/ArrayProperty.h"
#include "Core/Property/NumericProperty.h"
#include "Core/Property/ObjectProperty.h"
#include "Core/Property/StructProperty.h"
#include "Core/Types/PropertyTypes.h"
#include "Editor/Settings/EditorSettings.h"
#include "ImGui/imgui.h"
#include "imgui_internal.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Object/FName.h"
#include "Object/Object.h"
#include "Object/Reflection/UStruct.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
	constexpr float MinPropertyLabelColumnWidth = 80.0f;
	constexpr float MaxPropertyLabelColumnWidth = 320.0f;
	constexpr float ReflectionPropertyIndentWidth = 4.0f;
	constexpr float ReflectionTreeNodeIndentSpacing = 4.0f;

	float ClampPropertyLabelColumnWidth(float Width)
	{
		return std::clamp(Width, MinPropertyLabelColumnWidth, MaxPropertyLabelColumnWidth);
	}

	bool TryGetResizedColumnWidth(int ColumnIndex, float& OutWidth)
	{
		ImGuiTable* Table = ImGui::GetCurrentTable();
		if (!Table || ColumnIndex < 0 || ColumnIndex >= Table->ColumnsCount)
		{
			return false;
		}

		if (Table->ResizedColumn != ColumnIndex && Table->LastResizedColumn != ColumnIndex)
		{
			return false;
		}

		OutWidth = Table->Columns[ColumnIndex].WidthGiven;
		return OutWidth > 0.0f;
	}

	const char* GetDisplayName(const FPropertyValue& Prop)
	{
		const char* DisplayName = Prop.GetDisplayName();
		return DisplayName && *DisplayName ? DisplayName : Prop.GetName();
	}

	struct FArrayElementContext
	{
		const FArrayProperty::FArrayOps* ArrayOps = nullptr;
		void* ArrayPtr = nullptr;
		size_t Index = 0;
		bool bReadOnly = false;
		bool bDeleted = false;
	};

	bool RenderPropertyRow(
		FPropertyValue& Prop,
		bool bReadOnly,
		int32 Depth,
		const char* OverrideLabel = nullptr,
		FArrayElementContext* ArrayElementContext = nullptr);
	bool RenderStructRows(UStruct* StructType, void* StructPtr, UObject* Owner, int32 Depth, bool bInheritedReadOnly);

	void RenderIndentedText(const char* Label, int32 Depth)
	{
		ImGui::AlignTextToFramePadding();
		if (Depth > 0)
		{
			ImGui::Indent(Depth * ReflectionPropertyIndentWidth);
		}

		ImGui::TextUnformatted(Label);

		if (Depth > 0)
		{
			ImGui::Unindent(Depth * ReflectionPropertyIndentWidth);
		}
	}

	bool RenderSectionHeader(const char* Label, int32 Depth, bool bExpandable)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		if (Depth > 0)
		{
			ImGui::Indent(Depth * ReflectionPropertyIndentWidth);
		}

		ImGuiTreeNodeFlags Flags =
			ImGuiTreeNodeFlags_SpanAllColumns |
			ImGuiTreeNodeFlags_LabelSpanAllColumns;
		if (bExpandable)
		{
			Flags |= ImGuiTreeNodeFlags_DefaultOpen;
		}
		else
		{
			Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}

		const bool bOpen = ImGui::TreeNodeEx("##Section", Flags, "%s", Label);

		if (Depth > 0)
		{
			ImGui::Unindent(Depth * ReflectionPropertyIndentWidth);
		}

		return bExpandable && bOpen;
	}

	bool RenderArrayElementDeleteContextMenu(FArrayElementContext* Context, const char* PopupId)
	{
		if (!Context || Context->bDeleted)
		{
			return false;
		}

		bool bDeleted = false;
		if (ImGui::BeginPopupContextItem(PopupId))
		{
			const bool bCanDelete =
				!Context->bReadOnly &&
				Context->ArrayOps &&
				Context->ArrayOps->RemoveAt &&
				Context->ArrayPtr;
			if (ImGui::MenuItem("Delete Element", nullptr, false, bCanDelete))
			{
				Context->ArrayOps->RemoveAt(Context->ArrayPtr, Context->Index);
				Context->bDeleted = true;
				bDeleted = true;
			}
			ImGui::EndPopup();
		}

		return bDeleted;
	}

	bool RenderStructRows(UStruct* StructType, void* StructPtr, UObject* Owner, int32 Depth, bool bInheritedReadOnly)
	{
		bool bAnyChanged = false;
		if (!StructType || !StructPtr)
		{
			return false;
		}

		TArray<const FProperty*> Properties;
		StructType->GetPropertyRefs(Properties);
		for (const FProperty* Property : Properties)
		{
			if (!Property || !Property->GetValuePtrFor(StructPtr))
			{
				continue;
			}

			FPropertyValue Prop = Property->ToValue(StructPtr, Owner);
			const bool bReadOnly = bInheritedReadOnly || ((Property->Flags & PF_ReadOnly) != 0);

			ImGui::PushID(Prop.GetName());
			bAnyChanged |= RenderPropertyRow(Prop, bReadOnly, Depth);
			ImGui::PopID();
		}

		return bAnyChanged;
	}

	bool RenderArrayRows(
		FPropertyValue& Prop,
		bool bReadOnly,
		int32 Depth,
		const char* Label,
		FArrayElementContext* ArrayElementContext = nullptr)
	{
		const FArrayProperty* ArrayProperty = Prop.Property ? Prop.Property->AsArrayProperty() : nullptr;
		void* ArrayPtr = Prop.GetValuePtr();
		const FArrayProperty::FArrayOps* ArrayOps = ArrayProperty ? ArrayProperty->GetArrayOps() : nullptr;
		const FProperty* InnerProperty = ArrayProperty ? ArrayProperty->GetInnerProperty() : nullptr;
		if (!ArrayPtr || !ArrayOps || !ArrayOps->GetNum || !ArrayOps->GetElementPtr || !InnerProperty)
		{
			RenderSectionHeader(Label, Depth, false);
			return RenderArrayElementDeleteContextMenu(ArrayElementContext, "##ArrayElementContext");
		}

		const size_t Count = ArrayOps->GetNum(ArrayPtr);
		if (Count == 0)
		{
			RenderSectionHeader(Label, Depth, false);
			return RenderArrayElementDeleteContextMenu(ArrayElementContext, "##ArrayElementContext");
		}

		const bool bOpen = RenderSectionHeader(Label, Depth, true);
		if (RenderArrayElementDeleteContextMenu(ArrayElementContext, "##ArrayElementContext"))
		{
			if (bOpen)
			{
				ImGui::TreePop();
			}
			return true;
		}

		if (!bOpen)
		{
			return false;
		}

		bool bAnyChanged = false;
		for (size_t Index = 0; Index < Count; ++Index)
		{
			void* ElementPtr = ArrayOps->GetElementPtr(ArrayPtr, Index);
			if (!ElementPtr)
			{
				continue;
			}

			FPropertyValue ElementValue;
			ElementValue.Object = Prop.Object;
			ElementValue.Property = InnerProperty;
			ElementValue.ContainerPtr = ElementPtr;

			char ElementLabel[64];
			std::snprintf(ElementLabel, sizeof(ElementLabel), "Index [%zu]", Index);

			FArrayElementContext ElementContext;
			ElementContext.ArrayOps = ArrayOps;
			ElementContext.ArrayPtr = ArrayPtr;
			ElementContext.Index = Index;
			ElementContext.bReadOnly = bReadOnly;

			ImGui::PushID(static_cast<int>(Index));
			bAnyChanged |= RenderPropertyRow(ElementValue, bReadOnly, Depth + 1, ElementLabel, &ElementContext);
			ImGui::PopID();

			if (ElementContext.bDeleted)
			{
				bAnyChanged = true;
				break;
			}
		}

		ImGui::TreePop();
		return bAnyChanged;
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
		case EPropertyType::Enum:
		{
			const FEnum* EnumType = Prop.GetEnumType();
			void* ValuePtr = Prop.GetValuePtr();
			if (!EnumType || !EnumType->GetEntries() || EnumType->GetCount() == 0 || !ValuePtr)
			{
				return false;
			}

			int64 CurrentValue = 0;
			const uint32 EnumSize = EnumType->GetSize();
			std::memcpy(&CurrentValue, ValuePtr, std::min<size_t>(EnumSize, sizeof(CurrentValue)));
			const char* Preview = EnumType->GetNameByValue(CurrentValue);
			bool bChanged = false;
			if (ImGui::BeginCombo("##Value", Preview))
			{
				for (uint32 Index = 0; Index < EnumType->GetCount(); ++Index)
				{
					const int64 EntryValue = EnumType->GetValueAt(Index);
					const bool bSelected = (CurrentValue == EntryValue);
					if (ImGui::Selectable(EnumType->GetNameAt(Index), bSelected))
					{
						std::memcpy(ValuePtr, &EntryValue, std::min<size_t>(EnumSize, sizeof(EntryValue)));
						bChanged = true;
					}
					if (bSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			return bChanged;
		}
		default:
			ImGui::TextDisabled("(unsupported type)");
			return false;
		}
	}

	bool RenderPropertyValue(FPropertyValue& Prop, bool bReadOnly)
	{
		if (bReadOnly)
		{
			ImGui::BeginDisabled();
		}

		const bool bChanged = RenderValue(Prop);

		if (bReadOnly)
		{
			ImGui::EndDisabled();
		}

		return !bReadOnly && bChanged;
	}

	bool RenderPropertyRow(
		FPropertyValue& Prop,
		bool bReadOnly,
		int32 Depth,
		const char* OverrideLabel,
		FArrayElementContext* ArrayElementContext)
	{
		const char* Label = OverrideLabel ? OverrideLabel : GetDisplayName(Prop);
		switch (Prop.GetType())
		{
		case EPropertyType::Struct:
		{
			UStruct* StructType = Prop.GetStructType();
			void* StructPtr = Prop.GetValuePtr();
			if (!StructType || !StructPtr)
			{
				RenderSectionHeader(Label, Depth, false);
				return RenderArrayElementDeleteContextMenu(ArrayElementContext, "##ArrayElementContext");
			}

			const bool bOpen = RenderSectionHeader(Label, Depth, true);
			if (RenderArrayElementDeleteContextMenu(ArrayElementContext, "##ArrayElementContext"))
			{
				if (bOpen)
				{
					ImGui::TreePop();
				}
				return true;
			}

			if (!bOpen)
			{
				return false;
			}

			const bool bChanged = RenderStructRows(StructType, StructPtr, Prop.Object, Depth + 1, bReadOnly);
			ImGui::TreePop();
			return !bReadOnly && bChanged;
		}
		case EPropertyType::Array:
			return RenderArrayRows(Prop, bReadOnly, Depth, Label, ArrayElementContext);
		case EPropertyType::ObjectRef:
		{
			const FObjectProperty* ObjectProperty = Prop.Property ? Prop.Property->AsObjectProperty() : nullptr;
			void* ValuePtr = Prop.GetValuePtr();
			UObject* Object = ObjectProperty && ValuePtr
				? ObjectProperty->GetObjectValueFromValuePtr(ValuePtr)
				: nullptr;

			if (!Object)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				RenderIndentedText(Label, Depth);
				if (RenderArrayElementDeleteContextMenu(ArrayElementContext, "##ArrayElementLabelContext"))
				{
					return true;
				}
				ImGui::TableSetColumnIndex(1);
				ImGui::TextDisabled("None");
				return RenderArrayElementDeleteContextMenu(ArrayElementContext, "##ArrayElementContext");
			}

			const bool bOpen = RenderSectionHeader(Label, Depth, true);
			if (RenderArrayElementDeleteContextMenu(ArrayElementContext, "##ArrayElementContext"))
			{
				if (bOpen)
				{
					ImGui::TreePop();
				}
				return true;
			}

			if (!bOpen)
			{
				return false;
			}

			UClass* Class = Object->GetClass();
			const bool bChanged = RenderStructRows(Class, Object, Object, Depth + 1, bReadOnly);
			ImGui::TreePop();
			return !bReadOnly && bChanged;
		}
		default:
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			RenderIndentedText(Label, Depth);
			if (RenderArrayElementDeleteContextMenu(ArrayElementContext, "##ArrayElementLabelContext"))
			{
				return true;
			}
			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-FLT_MIN);
			const bool bChanged = RenderPropertyValue(Prop, bReadOnly);
			return RenderArrayElementDeleteContextMenu(ArrayElementContext, "##ArrayElementContext") || bChanged;
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
		FEditorSettings& Settings = FEditorSettings::Get();
		Settings.ReflectionPropertyLabelColumnWidth = ClampPropertyLabelColumnWidth(Settings.ReflectionPropertyLabelColumnWidth);

		const ImGuiTableFlags TableFlags =
			ImGuiTableFlags_SizingStretchProp |
			ImGuiTableFlags_BordersInnerV |
			ImGuiTableFlags_Resizable |
			ImGuiTableFlags_NoSavedSettings;
		if (ImGui::BeginTable(EffectiveTableId, 2, TableFlags))
		{
			ImGui::TableSetupColumn(
				"##label",
				ImGuiTableColumnFlags_WidthFixed,
				Settings.ReflectionPropertyLabelColumnWidth);
			ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);

			ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ReflectionTreeNodeIndentSpacing);
			bAnyChanged = RenderStructRows(StructType, StructPtr, Owner, 0, false);
			ImGui::PopStyleVar();

			float CurrentLabelWidth = 0.0f;
			if (TryGetResizedColumnWidth(0, CurrentLabelWidth))
			{
				const float NewLabelWidth = ClampPropertyLabelColumnWidth(CurrentLabelWidth);
				if (std::fabs(NewLabelWidth - Settings.ReflectionPropertyLabelColumnWidth) > 0.5f)
				{
					Settings.ReflectionPropertyLabelColumnWidth = NewLabelWidth;
				}
			}

			ImGui::EndTable();
		}

		return bAnyChanged;
	}
}
