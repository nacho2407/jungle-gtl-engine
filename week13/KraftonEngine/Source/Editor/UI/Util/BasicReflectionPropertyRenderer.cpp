#include "Editor/UI/Util/BasicReflectionPropertyRenderer.h"

#include "Core/Property/ArrayProperty.h"
#include "Core/Property/ClassProperty.h"
#include "Core/Property/NumericProperty.h"
#include "Core/Property/ObjectProperty.h"
#include "Core/Property/SoftObjectProperty.h"
#include "Core/Property/StructProperty.h"
#include "Core/Types/PropertyTypes.h"
#include "Math/Rotator.h"
#include "Object/FName.h"
#include "Object/Object.h"
#include "Object/Reflection/UClass.h"
#include "Object/Reflection/UStruct.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>

struct FBasicReflectionPropertyRenderer::FCategoryGroup
{
	FString Category;
	TArray<const FProperty*> Properties;
};

namespace
{
	constexpr float PropertyIndentWidth = 16.0f;

	bool StringEquals(const char* A, const char* B)
	{
		if (A == B)
		{
			return true;
		}
		if (!A || !B)
		{
			return false;
		}
		return std::strcmp(A, B) == 0;
	}

	const char* GetPropertyName(const FProperty* Property)
	{
		return Property && Property->Name ? Property->Name : "";
	}

	const char* GetPropertyDisplayName(const FPropertyValue& Value)
	{
		const char* DisplayName = Value.GetDisplayName();
		return DisplayName && *DisplayName ? DisplayName : Value.GetName();
	}

	FString AppendPath(const FString& Prefix, const char* Name)
	{
		if (Prefix.empty())
		{
			return Name ? FString(Name) : FString();
		}
		return Prefix + "." + (Name ? Name : "");
	}

	FString AppendArrayIndexPath(const FString& Prefix, size_t Index)
	{
		char Buffer[64];
		std::snprintf(Buffer, sizeof(Buffer), "[%zu]", Index);
		return Prefix + Buffer;
	}

	bool IsEditorVisibleProperty(const FProperty* Property, UObject* Owner)
	{
		if (!Property || (Property->Flags & PF_Edit) == 0)
		{
			return false;
		}
		if (Owner && !Owner->ShouldExposeProperty(*Property))
		{
			return false;
		}
		return true;
	}

	bool IsSamePropertyMeaning(const FProperty* A, const FProperty* B)
	{
		if (!A || !B)
		{
			return false;
		}
		if (!StringEquals(A->Name, B->Name) ||
			!StringEquals(A->OwnerClassName, B->OwnerClassName) ||
			A->GetType() != B->GetType())
		{
			return false;
		}

		switch (A->GetType())
		{
		case EPropertyType::Enum:
			return A->GetEnumType() == B->GetEnumType();
		case EPropertyType::Struct:
			return A->GetStructType() == B->GetStructType();
		case EPropertyType::Array:
		{
			const FArrayProperty* ArrayA = A->AsArrayProperty();
			const FArrayProperty* ArrayB = B->AsArrayProperty();
			if (!ArrayA || !ArrayB || ArrayA->GetElementType() != ArrayB->GetElementType())
			{
				return false;
			}
			const FProperty* InnerA = ArrayA->GetInnerProperty();
			const FProperty* InnerB = ArrayB->GetInnerProperty();
			return (!InnerA && !InnerB) || IsSamePropertyMeaning(InnerA, InnerB);
		}
		default:
			return true;
		}
	}

	const FProperty* FindCompatibleProperty(UStruct* StructType, const FProperty* SourceProperty, UObject* Owner)
	{
		if (!StructType || !SourceProperty)
		{
			return nullptr;
		}

		TArray<const FProperty*> Properties;
		StructType->GetPropertyRefs(Properties);
		for (const FProperty* Property : Properties)
		{
			if (!IsEditorVisibleProperty(Property, Owner))
			{
				continue;
			}
			if (IsSamePropertyMeaning(SourceProperty, Property))
			{
				return Property;
			}
		}
		return nullptr;
	}

	bool CopyPropertyValue(const FPropertyValue& SrcValue, FPropertyValue& DstValue)
	{
		void* SrcPtr = SrcValue.GetValuePtr();
		void* DstPtr = DstValue.GetValuePtr();
		if (!SrcPtr || !DstPtr || SrcValue.GetType() != DstValue.GetType())
		{
			return false;
		}

		const FSoftObjectProperty* SrcSoftProperty = SrcValue.Property ? SrcValue.Property->AsSoftObjectProperty() : nullptr;
		const FSoftObjectProperty* DstSoftProperty = DstValue.Property ? DstValue.Property->AsSoftObjectProperty() : nullptr;
		if (SrcSoftProperty || DstSoftProperty)
		{
			if (!SrcSoftProperty || !DstSoftProperty)
			{
				return false;
			}
			DstSoftProperty->SetPathFromValuePtr(DstPtr, SrcSoftProperty->GetPathFromValuePtr(SrcPtr));
			return true;
		}

		size_t Size = 0;
		switch (SrcValue.GetType())
		{
		case EPropertyType::Bool:
			Size = sizeof(bool);
			break;
		case EPropertyType::ByteBool:
			Size = sizeof(uint8);
			break;
		case EPropertyType::Int:
			Size = sizeof(int32);
			break;
		case EPropertyType::Float:
			Size = sizeof(float);
			break;
		case EPropertyType::Vec3:
		case EPropertyType::Rotator:
			Size = sizeof(float) * 3;
			break;
		case EPropertyType::Vec4:
		case EPropertyType::Color4:
			Size = sizeof(float) * 4;
			break;
		case EPropertyType::Enum:
			Size = SrcValue.GetEnumType() ? SrcValue.GetEnumType()->GetSize() : sizeof(int32);
			break;
		case EPropertyType::String:
			*static_cast<FString*>(DstPtr) = *static_cast<FString*>(SrcPtr);
			return true;
		case EPropertyType::Name:
			*static_cast<FName*>(DstPtr) = *static_cast<FName*>(SrcPtr);
			return true;
		case EPropertyType::ObjectRef:
		{
			const FObjectProperty* SrcObjectProperty = SrcValue.Property ? SrcValue.Property->AsObjectProperty() : nullptr;
			const FObjectProperty* DstObjectProperty = DstValue.Property ? DstValue.Property->AsObjectProperty() : nullptr;
			if (!SrcObjectProperty || !DstObjectProperty)
			{
				return false;
			}
			DstObjectProperty->SetObjectValueFromValuePtr(
				DstPtr,
				SrcObjectProperty->GetObjectValueFromValuePtr(SrcPtr)
			);
			return true;
		}
		case EPropertyType::ClassRef:
		{
			const FClassProperty* SrcClassProperty = SrcValue.Property ? SrcValue.Property->AsClassProperty() : nullptr;
			const FClassProperty* DstClassProperty = DstValue.Property ? DstValue.Property->AsClassProperty() : nullptr;
			if (!SrcClassProperty || !DstClassProperty)
			{
				return false;
			}
			DstClassProperty->SetClassValue(DstValue.ContainerPtr, SrcClassProperty->GetClassValue(SrcValue.ContainerPtr));
			return true;
		}
		default:
			return false;
		}

		if (Size > 0)
		{
			std::memcpy(DstPtr, SrcPtr, Size);
			return true;
		}
		return false;
	}

	void DispatchPostEditChange(const FPropertyValue& Value, const FString& PropertyPath)
	{
		if (!Value.Object)
		{
			return;
		}

		FPropertyChangedEvent Event;
		Event.Object = Value.Object;
		Event.Property = Value.Property;
		Event.PropertyName = Value.GetName();
		Event.DisplayName = GetPropertyDisplayName(Value);
		Event.PropertyPath = PropertyPath.empty() ? Value.GetName() : PropertyPath;
		Event.Type = Value.GetType();
		Event.ChangeType = EPropertyChangeType::ValueSet;
		Value.Object->PostEditChangeProperty(Event);
	}

	void RenderLabelCell(const char* Label, int32 Depth)
	{
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		if (Depth > 0)
		{
			ImGui::Indent(static_cast<float>(Depth) * PropertyIndentWidth);
		}
		ImGui::TextUnformatted(Label ? Label : "");
		if (Depth > 0)
		{
			ImGui::Unindent(static_cast<float>(Depth) * PropertyIndentWidth);
		}
	}

	bool RenderTreeLabelCell(const char* Label, int32 Depth)
	{
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		if (Depth > 0)
		{
			ImGui::Indent(static_cast<float>(Depth) * PropertyIndentWidth);
		}

		const ImGuiTreeNodeFlags Flags =
			ImGuiTreeNodeFlags_DefaultOpen |
			ImGuiTreeNodeFlags_SpanAvailWidth |
			ImGuiTreeNodeFlags_FramePadding |
			ImGuiTreeNodeFlags_NoTreePushOnOpen;
		const bool bOpen = ImGui::TreeNodeEx("##PropertyTree", Flags, "%s", Label ? Label : "");

		if (Depth > 0)
		{
			ImGui::Unindent(static_cast<float>(Depth) * PropertyIndentWidth);
		}
		return bOpen;
	}

	bool IsReadOnly(const FPropertyValue& Value)
	{
		return Value.Property && (Value.Property->Flags & PF_ReadOnly) != 0;
	}
}

bool FBasicReflectionPropertyRenderer::Render(const TArray<FSelectionDetailTarget>& Targets, const char* TableId)
{
	TArray<FSelectionDetailTarget> ValidTargets;
	for (const FSelectionDetailTarget& Target : Targets)
	{
		if (!Target.IsValidTarget())
		{
			continue;
		}
		if (Target.ObjectPtr)
		{
			Target.ObjectPtr->PreGetEditableProperties();
		}
		ValidTargets.push_back(Target);
	}

	if (ValidTargets.empty())
	{
		ImGui::TextDisabled("No object selected.");
		return false;
	}

	TArray<FCategoryGroup> Categories;
	CollectTopLevelCategories(ValidTargets.front(), ValidTargets, Categories);
	if (Categories.empty())
	{
		ImGui::TextDisabled("(no editable properties)");
		return false;
	}

	bool bAnyChanged = false;
	for (const FCategoryGroup& Category : Categories)
	{
		const char* CategoryName = Category.Category.empty() ? "Default" : Category.Category.c_str();
		if (!ImGui::CollapsingHeader(CategoryName, ImGuiTreeNodeFlags_DefaultOpen))
		{
			continue;
		}

		ImGui::PushID(CategoryName);
		bAnyChanged |= RenderCategory(Category, ValidTargets.front(), ValidTargets, TableId);
		ImGui::PopID();
	}
	return bAnyChanged;
}

void FBasicReflectionPropertyRenderer::CollectTopLevelCategories(
	const FSelectionDetailTarget& PrimaryTarget,
	const TArray<FSelectionDetailTarget>& Targets,
	TArray<FCategoryGroup>& OutCategories) const
{
	OutCategories.clear();
	if (!PrimaryTarget.IsValidTarget())
	{
		return;
	}

	TArray<const FProperty*> Properties;
	PrimaryTarget.StructType->GetPropertyRefs(Properties);
	for (const FProperty* Property : Properties)
	{
		if (!IsEditorVisibleProperty(Property, PrimaryTarget.ObjectPtr) ||
			!Property->GetValuePtrFor(PrimaryTarget.ContainerPtr))
		{
			continue;
		}

		bool bAllTargetsCompatible = true;
		for (size_t TargetIndex = 1; TargetIndex < Targets.size(); ++TargetIndex)
		{
			const FSelectionDetailTarget& Target = Targets[TargetIndex];
			const FProperty* CompatibleProperty = FindCompatibleProperty(Target.StructType, Property, Target.ObjectPtr);
			if (!CompatibleProperty || !CompatibleProperty->GetValuePtrFor(Target.ContainerPtr))
			{
				bAllTargetsCompatible = false;
				break;
			}
		}
		if (!bAllTargetsCompatible)
		{
			continue;
		}

		const FString Category = Property->Category ? Property->Category : "";
		auto CategoryIt = std::find_if(
			OutCategories.begin(),
			OutCategories.end(),
			[&Category](const FCategoryGroup& Group)
			{
				return Group.Category == Category;
			});
		if (CategoryIt == OutCategories.end())
		{
			FCategoryGroup Group;
			Group.Category = Category;
			Group.Properties.push_back(Property);
			OutCategories.push_back(Group);
		}
		else
		{
			CategoryIt->Properties.push_back(Property);
		}
	}
}

bool FBasicReflectionPropertyRenderer::RenderCategory(
	const FCategoryGroup& Category,
	const FSelectionDetailTarget& PrimaryTarget,
	const TArray<FSelectionDetailTarget>& Targets,
	const char* TableId)
{
	const ImGuiTableFlags TableFlags =
		ImGuiTableFlags_BordersInnerV |
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_SizingStretchProp;

	const FString CategoryTableId = FString(TableId ? TableId : "##BasicReflectionProperties") + Category.Category;
	if (!ImGui::BeginTable(CategoryTableId.c_str(), 2, TableFlags))
	{
		return false;
	}

	ImGui::TableSetupColumn("0", ImGuiTableColumnFlags_WidthFixed, 150.0f);
	ImGui::TableSetupColumn("1", ImGuiTableColumnFlags_WidthStretch);

	bool bAnyChanged = false;
	for (const FProperty* Property : Category.Properties)
	{
		FPropertyValue PrimaryValue = Property->ToValue(PrimaryTarget.ContainerPtr, PrimaryTarget.ObjectPtr);
		TArray<FPropertyValue> CompatibleValues;
		CompatibleValues.push_back(PrimaryValue);

		bool bAllTargetsCompatible = true;
		for (size_t TargetIndex = 1; TargetIndex < Targets.size(); ++TargetIndex)
		{
			const FSelectionDetailTarget& Target = Targets[TargetIndex];
			const FProperty* CompatibleProperty = FindCompatibleProperty(Target.StructType, Property, Target.ObjectPtr);
			if (!CompatibleProperty || !CompatibleProperty->GetValuePtrFor(Target.ContainerPtr))
			{
				bAllTargetsCompatible = false;
				break;
			}
			CompatibleValues.push_back(CompatibleProperty->ToValue(Target.ContainerPtr, Target.ObjectPtr));
		}

		if (!bAllTargetsCompatible)
		{
			continue;
		}

		ImGui::PushID(GetPropertyName(Property));
		bAnyChanged |= RenderPropertyRow(PrimaryValue, CompatibleValues, GetPropertyName(Property), 0);
		ImGui::PopID();
	}

	ImGui::EndTable();
	return bAnyChanged;
}

bool FBasicReflectionPropertyRenderer::RenderPropertyRow(
	FPropertyValue& PrimaryValue,
	const TArray<FPropertyValue>& CompatibleValues,
	const FString& PropertyPath,
	int32 Depth)
{
	switch (PrimaryValue.GetType())
	{
	case EPropertyType::Struct:
	{
		ImGui::TableNextRow();
		const bool bOpen = RenderTreeLabelCell(GetPropertyDisplayName(PrimaryValue), Depth);
		ImGui::TableSetColumnIndex(1);
		UStruct* StructType = PrimaryValue.GetStructType();
		ImGui::TextDisabled("%s", StructType ? StructType->GetName() : "(unsupported struct)");
		return bOpen && RenderStructChildren(PrimaryValue, CompatibleValues, PropertyPath, Depth + 1);
	}
	case EPropertyType::Array:
	{
		ImGui::TableNextRow();
		const bool bOpen = RenderTreeLabelCell(GetPropertyDisplayName(PrimaryValue), Depth);
		ImGui::TableSetColumnIndex(1);

		const FArrayProperty* ArrayProperty = PrimaryValue.Property ? PrimaryValue.Property->AsArrayProperty() : nullptr;
		void* ArrayPtr = PrimaryValue.GetValuePtr();
		const FArrayProperty::FArrayOps* Ops = ArrayProperty ? ArrayProperty->GetArrayOps() : nullptr;
		const size_t Count = (ArrayPtr && Ops && Ops->GetNum) ? Ops->GetNum(ArrayPtr) : 0;
		ImGui::TextDisabled("Array Elements: %zu", Count);
		return bOpen && RenderArrayChildren(PrimaryValue, CompatibleValues, PropertyPath, Depth + 1);
	}
	default:
		break;
	}

	ImGui::TableNextRow();
	RenderLabelCell(GetPropertyDisplayName(PrimaryValue), Depth);
	ImGui::TableSetColumnIndex(1);
	ImGui::SetNextItemWidth(-FLT_MIN);

	bool bChanged = RenderLeafValue(PrimaryValue, IsReadOnly(PrimaryValue));
	if (bChanged)
	{
		DispatchPostEditChange(PrimaryValue, PropertyPath);
		PropagateValueChange(PrimaryValue, CompatibleValues, PropertyPath);
	}
	return bChanged;
}

bool FBasicReflectionPropertyRenderer::RenderStructChildren(
	FPropertyValue& PrimaryValue,
	const TArray<FPropertyValue>& CompatibleValues,
	const FString& PropertyPath,
	int32 Depth)
{
	UStruct* StructType = PrimaryValue.GetStructType();
	void* StructPtr = PrimaryValue.GetValuePtr();
	if (!StructType || !StructPtr)
	{
		return false;
	}

	TArray<const FProperty*> ChildProperties;
	StructType->GetPropertyRefs(ChildProperties);

	bool bAnyChanged = false;
	for (const FProperty* ChildProperty : ChildProperties)
	{
		if (!IsEditorVisibleProperty(ChildProperty, PrimaryValue.Object) ||
			!ChildProperty->GetValuePtrFor(StructPtr))
		{
			continue;
		}

		FPropertyValue PrimaryChild = ChildProperty->ToValue(StructPtr, PrimaryValue.Object);
		TArray<FPropertyValue> CompatibleChildren;
		CompatibleChildren.push_back(PrimaryChild);

		bool bAllTargetsCompatible = true;
		for (size_t ValueIndex = 1; ValueIndex < CompatibleValues.size(); ++ValueIndex)
		{
			const FPropertyValue& OtherParent = CompatibleValues[ValueIndex];
			UStruct* OtherStructType = OtherParent.GetStructType();
			void* OtherStructPtr = OtherParent.GetValuePtr();
			const FProperty* OtherChildProperty = FindCompatibleProperty(OtherStructType, ChildProperty, OtherParent.Object);
			if (!OtherChildProperty || !OtherStructPtr || !OtherChildProperty->GetValuePtrFor(OtherStructPtr))
			{
				bAllTargetsCompatible = false;
				break;
			}
			CompatibleChildren.push_back(OtherChildProperty->ToValue(OtherStructPtr, OtherParent.Object));
		}

		if (!bAllTargetsCompatible)
		{
			continue;
		}

		ImGui::PushID(GetPropertyName(ChildProperty));
		bAnyChanged |= RenderPropertyRow(
			PrimaryChild,
			CompatibleChildren,
			AppendPath(PropertyPath, GetPropertyName(ChildProperty)),
			Depth
		);
		ImGui::PopID();
	}
	return bAnyChanged;
}

bool FBasicReflectionPropertyRenderer::RenderArrayChildren(
	FPropertyValue& PrimaryValue,
	const TArray<FPropertyValue>& CompatibleValues,
	const FString& PropertyPath,
	int32 Depth)
{
	const FArrayProperty* ArrayProperty = PrimaryValue.Property ? PrimaryValue.Property->AsArrayProperty() : nullptr;
	void* ArrayPtr = PrimaryValue.GetValuePtr();
	const FArrayProperty::FArrayOps* Ops = ArrayProperty ? ArrayProperty->GetArrayOps() : nullptr;
	const FProperty* InnerProperty = ArrayProperty ? ArrayProperty->GetInnerProperty() : nullptr;
	if (!ArrayPtr || !Ops || !Ops->GetNum || !Ops->GetElementPtr || !InnerProperty)
	{
		return false;
	}

	bool bAnyChanged = false;
	const size_t PrimaryCount = Ops->GetNum(ArrayPtr);
	for (size_t Index = 0; Index < PrimaryCount; ++Index)
	{
		void* ElementPtr = Ops->GetElementPtr(ArrayPtr, Index);
		if (!ElementPtr)
		{
			continue;
		}

		FPropertyValue PrimaryElement;
		PrimaryElement.Object = PrimaryValue.Object;
		PrimaryElement.Property = InnerProperty;
		PrimaryElement.ContainerPtr = ElementPtr;

		TArray<FPropertyValue> CompatibleElements;
		CompatibleElements.push_back(PrimaryElement);

		bool bAllTargetsCompatible = true;
		for (size_t ValueIndex = 1; ValueIndex < CompatibleValues.size(); ++ValueIndex)
		{
			const FPropertyValue& OtherArrayValue = CompatibleValues[ValueIndex];
			const FArrayProperty* OtherArrayProperty = OtherArrayValue.Property ? OtherArrayValue.Property->AsArrayProperty() : nullptr;
			void* OtherArrayPtr = OtherArrayValue.GetValuePtr();
			const FArrayProperty::FArrayOps* OtherOps = OtherArrayProperty ? OtherArrayProperty->GetArrayOps() : nullptr;
			const FProperty* OtherInnerProperty = OtherArrayProperty ? OtherArrayProperty->GetInnerProperty() : nullptr;
			if (!OtherArrayPtr || !OtherOps || !OtherOps->GetNum || !OtherOps->GetElementPtr ||
				!OtherInnerProperty || !IsSamePropertyMeaning(InnerProperty, OtherInnerProperty) ||
				Index >= OtherOps->GetNum(OtherArrayPtr))
			{
				bAllTargetsCompatible = false;
				break;
			}

			void* OtherElementPtr = OtherOps->GetElementPtr(OtherArrayPtr, Index);
			if (!OtherElementPtr)
			{
				bAllTargetsCompatible = false;
				break;
			}

			FPropertyValue OtherElement;
			OtherElement.Object = OtherArrayValue.Object;
			OtherElement.Property = OtherInnerProperty;
			OtherElement.ContainerPtr = OtherElementPtr;
			CompatibleElements.push_back(OtherElement);
		}

		if (!bAllTargetsCompatible)
		{
			continue;
		}

		char Label[64];
		std::snprintf(Label, sizeof(Label), "Element %zu", Index);
		ImGui::PushID(static_cast<int>(Index));
		bAnyChanged |= RenderPropertyRow(
			PrimaryElement,
			CompatibleElements,
			AppendArrayIndexPath(PropertyPath, Index),
			Depth
		);
		ImGui::PopID();
	}
	return bAnyChanged;
}

bool FBasicReflectionPropertyRenderer::RenderLeafValue(FPropertyValue& Value, bool bReadOnly)
{
	void* ValuePtr = Value.GetValuePtr();
	if (!ValuePtr)
	{
		ImGui::TextDisabled("(null)");
		return false;
	}

	if (bReadOnly)
	{
		ImGui::BeginDisabled();
	}

	bool bChanged = false;
	switch (Value.GetType())
	{
	case EPropertyType::Bool:
		bChanged = ImGui::Checkbox("##Value", static_cast<bool*>(ValuePtr));
		break;
	case EPropertyType::ByteBool:
	{
		uint8* ByteValue = static_cast<uint8*>(ValuePtr);
		bool bValue = *ByteValue != 0;
		if (ImGui::Checkbox("##Value", &bValue))
		{
			*ByteValue = bValue ? 1 : 0;
			bChanged = true;
		}
		break;
	}
	case EPropertyType::Int:
	{
		const FNumericProperty* NumericProperty = Value.Property ? Value.Property->AsNumericProperty() : nullptr;
		const float Min = NumericProperty ? NumericProperty->GetMin() : Value.GetMin();
		const float Max = NumericProperty ? NumericProperty->GetMax() : Value.GetMax();
		const float Speed = NumericProperty ? NumericProperty->GetSpeed() : Value.GetSpeed();
		int32* IntValue = static_cast<int32*>(ValuePtr);
		bChanged = (Min != 0.0f || Max != 0.0f)
			? ImGui::DragInt("##Value", IntValue, Speed, static_cast<int32>(Min), static_cast<int32>(Max))
			: ImGui::DragInt("##Value", IntValue, Speed);
		break;
	}
	case EPropertyType::Float:
	{
		const FNumericProperty* NumericProperty = Value.Property ? Value.Property->AsNumericProperty() : nullptr;
		const float Min = NumericProperty ? NumericProperty->GetMin() : Value.GetMin();
		const float Max = NumericProperty ? NumericProperty->GetMax() : Value.GetMax();
		const float Speed = NumericProperty ? NumericProperty->GetSpeed() : Value.GetSpeed();
		float* FloatValue = static_cast<float*>(ValuePtr);
		bChanged = (Min != 0.0f || Max != 0.0f)
			? ImGui::DragFloat("##Value", FloatValue, Speed, Min, Max, "%.4f")
			: ImGui::DragFloat("##Value", FloatValue, Speed);
		break;
	}
	case EPropertyType::Vec3:
		bChanged = ImGui::DragFloat3("##Value", static_cast<float*>(ValuePtr), Value.GetSpeed());
		break;
	case EPropertyType::Vec4:
		bChanged = ImGui::DragFloat4("##Value", static_cast<float*>(ValuePtr), Value.GetSpeed());
		break;
	case EPropertyType::Color4:
		bChanged = ImGui::ColorEdit4("##Value", static_cast<float*>(ValuePtr));
		break;
	case EPropertyType::Rotator:
	{
		FRotator* Rotator = static_cast<FRotator*>(ValuePtr);
		float Rotation[3] = { Rotator->Roll, Rotator->Pitch, Rotator->Yaw };
		if (ImGui::DragFloat3("##Value", Rotation, Value.GetSpeed()))
		{
			Rotator->Roll = Rotation[0];
			Rotator->Pitch = Rotation[1];
			Rotator->Yaw = Rotation[2];
			bChanged = true;
		}
		break;
	}
	case EPropertyType::String:
	{
		FString* StringValue = static_cast<FString*>(ValuePtr);
		char Buffer[256];
		strncpy_s(Buffer, sizeof(Buffer), StringValue->c_str(), _TRUNCATE);
		if (ImGui::InputText("##Value", Buffer, sizeof(Buffer)))
		{
			*StringValue = Buffer;
			bChanged = true;
		}
		break;
	}
	case EPropertyType::Name:
	{
		FName* NameValue = static_cast<FName*>(ValuePtr);
		FString CurrentValue = NameValue->ToString();
		char Buffer[256];
		strncpy_s(Buffer, sizeof(Buffer), CurrentValue.c_str(), _TRUNCATE);
		if (ImGui::InputText("##Value", Buffer, sizeof(Buffer)))
		{
			*NameValue = FName(FString(Buffer));
			bChanged = true;
		}
		break;
	}
	case EPropertyType::Enum:
	{
		const FEnum* EnumType = Value.GetEnumType();
		if (!EnumType || !EnumType->GetEntries() || EnumType->GetCount() == 0)
		{
			ImGui::TextDisabled("(invalid enum)");
			break;
		}

		int64 CurrentValue = 0;
		const uint32 EnumSize = EnumType->GetSize();
		std::memcpy(&CurrentValue, ValuePtr, std::min<size_t>(EnumSize, sizeof(CurrentValue)));
		const char* Preview = EnumType->GetNameByValue(CurrentValue);
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
		break;
	}
	case EPropertyType::SoftObjectRef:
	{
		const FSoftObjectProperty* SoftProperty = Value.Property ? Value.Property->AsSoftObjectProperty() : nullptr;
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
			bChanged = true;
		}
		break;
	}
	case EPropertyType::ObjectRef:
	{
		const FObjectProperty* ObjectProperty = Value.Property ? Value.Property->AsObjectProperty() : nullptr;
		UObject* Object = ObjectProperty ? ObjectProperty->GetObjectValueFromValuePtr(ValuePtr) : nullptr;
		ImGui::TextDisabled("%s", Object ? Object->GetName().c_str() : "None");
		break;
	}
	case EPropertyType::ClassRef:
	{
		const FClassProperty* ClassProperty = Value.Property ? Value.Property->AsClassProperty() : nullptr;
		UClass* Class = ClassProperty ? ClassProperty->GetClassValueFromValuePtr(ValuePtr) : nullptr;
		ImGui::TextDisabled("%s", Class ? Class->GetName() : "None");
		break;
	}
	default:
		ImGui::TextDisabled("(unsupported)");
		break;
	}

	if (bReadOnly)
	{
		ImGui::EndDisabled();
		return false;
	}
	return bChanged;
}

void FBasicReflectionPropertyRenderer::PropagateValueChange(
	const FPropertyValue& SourceValue,
	const TArray<FPropertyValue>& CompatibleValues,
	const FString& PropertyPath) const
{
	for (size_t Index = 1; Index < CompatibleValues.size(); ++Index)
	{
		FPropertyValue TargetValue = CompatibleValues[Index];
		if (CopyPropertyValue(SourceValue, TargetValue))
		{
			DispatchPostEditChange(TargetValue, PropertyPath);
		}
	}
}
