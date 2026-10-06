#pragma once

#include "Core/Property/ArrayProperty.h"
#include "Core/Property/ClassProperty.h"
#include "Core/Property/NumericProperty.h"
#include "Core/Property/ObjectProperty.h"
#include "Core/Property/SoftObjectProperty.h"
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

namespace FDetailPropertyRenderer
{
	namespace Private
	{
		constexpr float MinPropertyLabelColumnWidth = 80.0f;
		constexpr float MaxPropertyLabelColumnWidth = 320.0f;
		constexpr float DetailPropertyIndentWidth = 16.0f;

		struct FArrayElementContext
		{
			const FArrayProperty::FArrayOps* ArrayOps = nullptr;
			void* ArrayPtr = nullptr;
			size_t Index = 0;
			bool bReadOnly = false;
			bool bDeleted = false;
		};

		inline float ClampLabelColumnWidth(float Width)
		{
			return std::clamp(Width, MinPropertyLabelColumnWidth, MaxPropertyLabelColumnWidth);
		}

		inline bool IsVisibleProperty(const FProperty* Property)
		{
			return Property && ((Property->Flags & PF_Edit) != 0);
		}

		inline const char* GetDisplayName(const FPropertyValue& Prop)
		{
			const char* DisplayName = Prop.GetDisplayName();
			return DisplayName && *DisplayName ? DisplayName : Prop.GetName();
		}

		inline bool TryGetResizedColumnWidth(int ColumnIndex, float& OutWidth)
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

		inline bool RenderArrayElementContextMenu(FArrayElementContext* Context, const char* PopupId)
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

		inline void RenderLabelCell(const char* Label, int32 Depth)
		{
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			if (Depth > 0)
			{
				ImGui::Indent(Depth * DetailPropertyIndentWidth);
			}

			ImGui::TextUnformatted(Label);

			if (Depth > 0)
			{
				ImGui::Unindent(Depth * DetailPropertyIndentWidth);
			}
		}

		inline bool RenderTreeLabelCell(const char* Label, int32 Depth)
		{
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			if (Depth > 0)
			{
				ImGui::Indent(Depth * DetailPropertyIndentWidth);
			}

			const ImGuiTreeNodeFlags Flags =
				ImGuiTreeNodeFlags_DefaultOpen |
				ImGuiTreeNodeFlags_SpanAvailWidth |
				ImGuiTreeNodeFlags_FramePadding |
				ImGuiTreeNodeFlags_NoTreePushOnOpen;
			const bool bOpen = ImGui::TreeNodeEx("##TreeLabel", Flags, "%s", Label);

			if (Depth > 0)
			{
				ImGui::Unindent(Depth * DetailPropertyIndentWidth);
			}
			return bOpen;
		}

		inline bool RenderPropertyRow(
			FPropertyValue& Prop,
			bool bReadOnly,
			const char* Label,
			int32 Depth,
			FArrayElementContext* ArrayContext = nullptr);

		inline bool RenderStructRows(UStruct* StructType, void* StructPtr, UObject* Owner, bool bReadOnly, int32 Depth)
		{
			if (!StructType || !StructPtr)
			{
				return false;
			}

			bool bAnyChanged = false;
			TArray<const FProperty*> Properties;
			StructType->GetPropertyRefs(Properties);
			for (const FProperty* Property : Properties)
			{
				if (!IsVisibleProperty(Property) || !Property->GetValuePtrFor(StructPtr))
				{
					continue;
				}

				FPropertyValue Child = Property->ToValue(StructPtr, Owner);
				const bool bChildReadOnly = bReadOnly || ((Property->Flags & PF_ReadOnly) != 0);

				ImGui::PushID(Child.GetName());
				bAnyChanged |= RenderPropertyRow(Child, bChildReadOnly, GetDisplayName(Child), Depth);
				ImGui::PopID();

				if (bAnyChanged)
				{
					break;
				}
			}
			return bAnyChanged;
		}

		inline bool RenderBasicValue(FPropertyValue& Prop)
		{
			void* ValuePtr = Prop.GetValuePtr();
			if (!ValuePtr)
			{
				ImGui::TextDisabled("(null)");
				return false;
			}

			switch (Prop.GetType())
			{
			case EPropertyType::Bool:
				return ImGui::Checkbox("##Value", static_cast<bool*>(ValuePtr));
			case EPropertyType::ByteBool:
			{
				uint8* Value = static_cast<uint8*>(ValuePtr);
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
				const FNumericProperty* NumericProperty = Prop.Property ? Prop.Property->AsNumericProperty() : nullptr;
				const float Min = NumericProperty ? NumericProperty->GetMin() : Prop.GetMin();
				const float Max = NumericProperty ? NumericProperty->GetMax() : Prop.GetMax();
				const float Speed = NumericProperty ? NumericProperty->GetSpeed() : Prop.GetSpeed();
				int32* Value = static_cast<int32*>(ValuePtr);
				return (Min != 0.0f || Max != 0.0f)
					? ImGui::DragInt("##Value", Value, Speed, static_cast<int32>(Min), static_cast<int32>(Max))
					: ImGui::DragInt("##Value", Value, Speed);
			}
			case EPropertyType::Float:
			{
				const FNumericProperty* NumericProperty = Prop.Property ? Prop.Property->AsNumericProperty() : nullptr;
				const float Min = NumericProperty ? NumericProperty->GetMin() : Prop.GetMin();
				const float Max = NumericProperty ? NumericProperty->GetMax() : Prop.GetMax();
				const float Speed = NumericProperty ? NumericProperty->GetSpeed() : Prop.GetSpeed();
				float* Value = static_cast<float*>(ValuePtr);
				return (Min != 0.0f || Max != 0.0f)
					? ImGui::DragFloat("##Value", Value, Speed, Min, Max, "%.4f")
					: ImGui::DragFloat("##Value", Value, Speed);
			}
			case EPropertyType::Vec3:
				return ImGui::DragFloat3("##Value", static_cast<float*>(ValuePtr), Prop.GetSpeed());
			case EPropertyType::Vec4:
				return ImGui::DragFloat4("##Value", static_cast<float*>(ValuePtr), Prop.GetSpeed());
			case EPropertyType::Color4:
				return ImGui::ColorEdit4("##Value", static_cast<float*>(ValuePtr));
			case EPropertyType::Rotator:
			{
				FRotator* Rotator = static_cast<FRotator*>(ValuePtr);
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
				FString* Value = static_cast<FString*>(ValuePtr);
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
				FName* Value = static_cast<FName*>(ValuePtr);
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
				if (!EnumType || !EnumType->GetEntries() || EnumType->GetCount() == 0)
				{
					ImGui::TextDisabled("(invalid enum)");
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
						const bool bSelected = CurrentValue == EntryValue;
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
			case EPropertyType::SoftObjectRef:
			{
				const FSoftObjectProperty* SoftProperty = Prop.Property ? Prop.Property->AsSoftObjectProperty() : nullptr;
				FString CurrentPath = SoftProperty
					? SoftProperty->GetPathFromValuePtr(ValuePtr)
					: *static_cast<FString*>(ValuePtr);
				char Buffer[512];
				strncpy_s(Buffer, sizeof(Buffer), CurrentPath.c_str(), _TRUNCATE);
				if (ImGui::InputText("##Value", Buffer, sizeof(Buffer)))
				{
					if (SoftProperty)
					{
						SoftProperty->SetPathFromValuePtr(ValuePtr, Buffer);
					}
					else
					{
						*static_cast<FString*>(ValuePtr) = Buffer;
					}
					return true;
				}
				return false;
			}
			case EPropertyType::ObjectRef:
			{
				const FObjectProperty* ObjectProperty = Prop.Property ? Prop.Property->AsObjectProperty() : nullptr;
				UObject* Object = ObjectProperty ? ObjectProperty->GetObjectValueFromValuePtr(ValuePtr) : nullptr;
				ImGui::TextDisabled("%s", Object ? Object->GetName().c_str() : "None");
				return false;
			}
			case EPropertyType::ClassRef:
			{
				const FClassProperty* ClassProperty = Prop.Property ? Prop.Property->AsClassProperty() : nullptr;
				UClass* Class = ClassProperty ? ClassProperty->GetClassValueFromValuePtr(ValuePtr) : nullptr;
				ImGui::TextDisabled("%s", Class ? Class->GetName() : "None");
				return false;
			}
			default:
				ImGui::TextDisabled("(unsupported)");
				return false;
			}
		}

		inline bool RenderLeafValue(FPropertyValue& Prop, bool bReadOnly)
		{
			if (bReadOnly)
			{
				ImGui::BeginDisabled();
			}

			const bool bChanged = RenderBasicValue(Prop);

			if (bReadOnly)
			{
				ImGui::EndDisabled();
				return false;
			}
			return bChanged;
		}

		inline bool RenderStructRow(
			FPropertyValue& Prop,
			bool bReadOnly,
			const char* Label,
			int32 Depth,
			FArrayElementContext* ArrayContext)
		{
			ImGui::TableNextRow();

			UStruct* StructType = Prop.GetStructType();
			void* StructPtr = Prop.GetValuePtr();
			if (!StructType || !StructPtr)
			{
				RenderLabelCell(Label, Depth);
				if (RenderArrayElementContextMenu(ArrayContext, "##ArrayElementLabelContext"))
				{
					return true;
				}
				ImGui::TableSetColumnIndex(1);
				ImGui::TextDisabled("(unsupported struct)");
				return RenderArrayElementContextMenu(ArrayContext, "##ArrayElementValueContext");
			}

			const bool bOpen = RenderTreeLabelCell(Label, Depth);
			if (RenderArrayElementContextMenu(ArrayContext, "##ArrayElementLabelContext"))
			{
				return true;
			}
			ImGui::TableSetColumnIndex(1);
			ImGui::TextDisabled("%s", StructType->GetName());
			if (RenderArrayElementContextMenu(ArrayContext, "##ArrayElementValueContext"))
			{
				return true;
			}
			return bOpen && RenderStructRows(StructType, StructPtr, Prop.Object, bReadOnly, Depth + 1);
		}

		inline bool RenderArrayRow(
			FPropertyValue& Prop,
			bool bReadOnly,
			const char* Label,
			int32 Depth,
			FArrayElementContext* ArrayContext)
		{
			ImGui::TableNextRow();

			const FArrayProperty* ArrayProperty = Prop.Property ? Prop.Property->AsArrayProperty() : nullptr;
			void* ArrayPtr = Prop.GetValuePtr();
			const FArrayProperty::FArrayOps* Ops = ArrayProperty ? ArrayProperty->GetArrayOps() : nullptr;
			const FProperty* InnerProperty = ArrayProperty ? ArrayProperty->GetInnerProperty() : nullptr;
			if (!ArrayPtr || !Ops || !Ops->GetNum || !Ops->GetElementPtr || !InnerProperty)
			{
				RenderLabelCell(Label, Depth);
				if (RenderArrayElementContextMenu(ArrayContext, "##ArrayElementLabelContext"))
				{
					return true;
				}
				ImGui::TableSetColumnIndex(1);
				ImGui::TextDisabled("(unsupported array)");
				return RenderArrayElementContextMenu(ArrayContext, "##ArrayElementValueContext");
			}

			const size_t Count = Ops->GetNum(ArrayPtr);
			if (Count == 0)
			{
				RenderLabelCell(Label, Depth);
				if (RenderArrayElementContextMenu(ArrayContext, "##ArrayElementLabelContext"))
				{
					return true;
				}
				ImGui::TableSetColumnIndex(1);
				ImGui::TextDisabled("Array Elements: 0");
				return RenderArrayElementContextMenu(ArrayContext, "##ArrayElementValueContext");
			}

			const bool bOpen = RenderTreeLabelCell(Label, Depth);
			if (RenderArrayElementContextMenu(ArrayContext, "##ArrayElementLabelContext"))
			{
				return true;
			}
			ImGui::TableSetColumnIndex(1);
			ImGui::TextDisabled("Array Elements: %zu", Count);
			if (RenderArrayElementContextMenu(ArrayContext, "##ArrayElementValueContext"))
			{
				return true;
			}
			if (!bOpen)
			{
				return false;
			}

			bool bAnyChanged = false;
			for (size_t Index = 0; Index < Count; ++Index)
			{
				void* ElementPtr = Ops->GetElementPtr(ArrayPtr, Index);
				if (!ElementPtr)
				{
					continue;
				}

				FPropertyValue Element;
				Element.Object = Prop.Object;
				Element.Property = InnerProperty;
				Element.ContainerPtr = ElementPtr;

				char Label[64];
				std::snprintf(Label, sizeof(Label), "Element %zu", Index);

				FArrayElementContext ElementContext;
				ElementContext.ArrayOps = Ops;
				ElementContext.ArrayPtr = ArrayPtr;
				ElementContext.Index = Index;
				ElementContext.bReadOnly = bReadOnly;

				ImGui::PushID(static_cast<int>(Index));
				bAnyChanged |= RenderPropertyRow(Element, bReadOnly, Label, Depth + 1, &ElementContext);
				ImGui::PopID();

				if (ElementContext.bDeleted)
				{
					bAnyChanged = true;
					break;
				}
			}
			return bAnyChanged;
		}

		inline bool RenderPropertyRow(
			FPropertyValue& Prop,
			bool bReadOnly,
			const char* Label,
			int32 Depth,
			FArrayElementContext* ArrayContext)
		{
			switch (Prop.GetType())
			{
			case EPropertyType::Struct:
				return RenderStructRow(Prop, bReadOnly, Label, Depth, ArrayContext);
			case EPropertyType::Array:
				return RenderArrayRow(Prop, bReadOnly, Label, Depth, ArrayContext);
			default:
				break;
			}

			ImGui::TableNextRow();
			RenderLabelCell(Label, Depth);
			if (RenderArrayElementContextMenu(ArrayContext, "##ArrayElementLabelContext"))
			{
				return true;
			}

			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-FLT_MIN);

			bool bChanged = false;
			bChanged = RenderLeafValue(Prop, bReadOnly);
			bChanged = RenderArrayElementContextMenu(ArrayContext, "##ArrayElementValueContext") || bChanged;
			return bChanged;
		}
	}

	inline bool RenderStructProperties(UStruct* StructType, void* StructPtr, UObject* Owner, const char* TableId)
	{
		if (!StructType || !StructPtr)
		{
			ImGui::TextDisabled("(no editable properties)");
			return false;
		}

		TArray<const FProperty*> Properties;
		StructType->GetPropertyRefs(Properties);

		TArray<FString> Categories;
		for (const FProperty* Property : Properties)
		{
			if (!Private::IsVisibleProperty(Property) || !Property->GetValuePtrFor(StructPtr))
			{
				continue;
			}

			FString Category = Property->Category ? Property->Category : "";
			bool bFound = false;
			for (const FString& Existing : Categories)
			{
				if (Existing == Category)
				{
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				Categories.push_back(Category);
			}
		}

		if (Categories.empty())
		{
			ImGui::TextDisabled("(no editable properties)");
			return false;
		}

		bool bAnyChanged = false;
		FEditorSettings& Settings = FEditorSettings::Get();
		Settings.ReflectionPropertyLabelColumnWidth =
			Private::ClampLabelColumnWidth(Settings.ReflectionPropertyLabelColumnWidth);

		for (const FString& Category : Categories)
		{
			if (!Category.empty())
			{
				if (!ImGui::CollapsingHeader(Category.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
				{
					continue;
				}
			}

			FString TableName = TableId ? TableId : "##DetailProperties";
			TableName += "_";
			TableName += Category.empty() ? "Default" : Category;
			const ImGuiTableFlags TableFlags =
				ImGuiTableFlags_SizingStretchProp |
				ImGuiTableFlags_BordersInnerV |
				ImGuiTableFlags_PadOuterX |
				ImGuiTableFlags_Resizable |
				ImGuiTableFlags_NoSavedSettings;

			if (!ImGui::BeginTable(TableName.c_str(), 2, TableFlags))
			{
				continue;
			}

			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, Settings.ReflectionPropertyLabelColumnWidth);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

			for (const FProperty* Property : Properties)
			{
				if (!Private::IsVisibleProperty(Property) || !Property->GetValuePtrFor(StructPtr))
				{
					continue;
				}

				const FString PropertyCategory = Property->Category ? Property->Category : "";
				if (PropertyCategory != Category)
				{
					continue;
				}

				FPropertyValue Prop = Property->ToValue(StructPtr, Owner);
				const bool bReadOnly = (Property->Flags & PF_ReadOnly) != 0;

				ImGui::PushID(Prop.GetName());
				bAnyChanged |= Private::RenderPropertyRow(Prop, bReadOnly, Private::GetDisplayName(Prop), 0);
				ImGui::PopID();

				if (bAnyChanged)
				{
					break;
				}
			}

			float CurrentLabelWidth = 0.0f;
			if (Private::TryGetResizedColumnWidth(0, CurrentLabelWidth))
			{
				const float NewLabelWidth = Private::ClampLabelColumnWidth(CurrentLabelWidth);
				if (std::abs(NewLabelWidth - Settings.ReflectionPropertyLabelColumnWidth) > 0.5f)
				{
					Settings.ReflectionPropertyLabelColumnWidth = NewLabelWidth;
				}
			}

			ImGui::EndTable();

			if (bAnyChanged)
			{
				break;
			}
		}

		return bAnyChanged;
	}
}
