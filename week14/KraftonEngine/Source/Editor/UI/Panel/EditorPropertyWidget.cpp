#include "Editor/UI/Panel/EditorPropertyWidget.h"
#include "Editor/EditorEngine.h"

#include "ImGui/imgui.h"
#include "Component/ActorComponent.h"
#include "Component/Primitive/BillboardComponent.h"
#include "Component/MeshComponent.h"
#include "Component/Movement/MovementComponent.h"
#include "Component/Debug/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Component/Primitive/TextRenderComponent.h"
#include "Component/Light/LightComponentBase.h"
#include "Component/Primitive/DecalComponent.h"
#include "Component/Primitive/HeightFogComponent.h"
#include "GameFramework/AActor.h"
#include "Asset/AssetRegistry.h"
#include "Core/Property/ClassProperty.h"
#include "Core/Property/ArrayProperty.h"
#include "Core/Property/NumericProperty.h"
#include "Core/Property/ObjectProperty.h"
#include "Core/Property/StructProperty.h"
#include "Core/Property/SoftObjectProperty.h"
#include "Core/Types/ClassTypes.h"
#include "Math/FloatCurve.h"
#include "Lua/LuaScriptManager.h"
#include "Resource/ResourceManager.h"
#include "Object/FName.h"
#include "Object/ObjectIterator.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Materials/Material.h"
#include "Mesh/Importer/MeshImportOptions.h"
#include "Mesh/MeshManager.h"
#include "Mesh/Static/StaticMesh.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Editor/UI/Asset/Mesh/MeshEditorWidget.h"
#include "Platform/Paths.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Serialization/MemoryArchive.h"

#include <Windows.h>
#include <commdlg.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cfloat>
#include <cstring>
#include <filesystem>
#include <cstdio>
#include <utility>

#include "Materials/MaterialManager.h"

#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

namespace
{
	constexpr float PropertyNameColumnMinWidth = 90.0f;
	constexpr float PropertyNameColumnMaxDefaultWidth = 275.0f;
	constexpr float PropertyNameColumnDefaultRatio = 0.45f;

	/**
	* @brief Details 패널 폭에 맞춘 속성 이름 컬럼 기본 폭을 계산합니다.
	*/
	float GetDetailsPropertyNameColumnWidth()
	{
		const float AvailableWidth = ImGui::GetContentRegionAvail().x;
		if (AvailableWidth <= 0.0f)
		{
			return PropertyNameColumnMaxDefaultWidth;
		}

		// 값 입력 영역 최소 확보
		const float MaxWidthByPanel = std::max(PropertyNameColumnMinWidth, AvailableWidth - PropertyNameColumnMinWidth);
		const float MaxWidth = std::min(PropertyNameColumnMaxDefaultWidth, MaxWidthByPanel);
		const float TargetWidth = AvailableWidth * PropertyNameColumnDefaultRatio;
		return std::clamp(TargetWidth, PropertyNameColumnMinWidth, MaxWidth);
	}

	/**
	* @brief Details 속성 테이블 공통 flag 집합을 반환합니다.
	*/
	ImGuiTableFlags GetDetailsPropertyTableFlags()
	{
		return ImGuiTableFlags_SizingStretchProp
			| ImGuiTableFlags_BordersInnerV
			| ImGuiTableFlags_PadOuterX
			| ImGuiTableFlags_RowBg
			| ImGuiTableFlags_Resizable;
	}

	bool ShouldHideInComponentTree(const UActorComponent* Component, bool bShowEditorOnlyComponents)
	{
		if (!Component)
		{
			return true;
		}

		return Component->IsHiddenInComponentTree()
			&& !(bShowEditorOnlyComponents && Component->IsEditorOnlyComponent());
	}

	bool DoesActorOwnComponent(AActor* Actor, const UActorComponent* Component)
	{
		if (!Actor || !IsValid(Component) || Component->GetOwner() != Actor)
		{
			return false;
		}

		for (UActorComponent* OwnedComponent : Actor->GetComponents())
		{
			if (OwnedComponent == Component)
			{
				return true;
			}
		}

		return false;
	}

	FString MakeComponentStructureDebugName(const char* Prefix, UActorComponent* Component)
	{
		const char* SafePrefix = Prefix ? Prefix : "Edit Component Structure";
		if (!IsValid(Component))
		{
			return FString(SafePrefix);
		}

		FString Name = Component->GetFName().ToString();
		if (Name.empty())
		{
			Name = Component->GetClass()->GetName();
		}

		return FString(SafePrefix) + ": " + Name;
	}

	struct FComponentClassGroup
	{
		const char* Label = nullptr;
		UClass* AnchorClass = nullptr;
		TArray<UClass*> Classes;
	};

	struct FDeferredPostEditChange
	{
		UObject* Object = nullptr;
		const FProperty* Property = nullptr;
		FString PropertyName;
		FString DisplayName;
		EPropertyType Type = EPropertyType::Bool;
	};

	void AddUniqueObject(TArray<UObject*>& Objects, UObject* Object)
	{
		if (!IsValid(Object))
		{
			return;
		}

		for (UObject* Existing : Objects)
		{
			if (Existing == Object)
			{
				return;
			}
		}

		Objects.push_back(Object);
	}

	TArray<uint32> MakeObjectUUIDList(const TArray<UObject*>& Objects)
	{
		TArray<uint32> UUIDs;
		for (UObject* Object : Objects)
		{
			if (!IsValid(Object) || Object->GetUUID() == 0)
			{
				continue;
			}

			bool bAlreadyAdded = false;
			for (uint32 ExistingUUID : UUIDs)
			{
				if (ExistingUUID == Object->GetUUID())
				{
					bAlreadyAdded = true;
					break;
				}
			}
			if (!bAlreadyAdded)
			{
				UUIDs.push_back(Object->GetUUID());
			}
		}
		return UUIDs;
	}

	bool AreObjectUUIDListsEqual(const TArray<uint32>& A, const TArray<uint32>& B)
	{
		if (A.size() != B.size())
		{
			return false;
		}

		for (size_t Index = 0; Index < A.size(); ++Index)
		{
			if (A[Index] != B[Index])
			{
				return false;
			}
		}
		return true;
	}

	TArray<UObject*> ResolveObjectsByUUIDs(const TArray<uint32>& UUIDs)
	{
		TArray<UObject*> Objects;
		for (uint32 UUID : UUIDs)
		{
			if (UUID == 0)
			{
				continue;
			}

			UObject* Object = UObjectManager::Get().FindByUUID(UUID);
			AddUniqueObject(Objects, Object);
		}
		return Objects;
	}

	FString MakeDetailsPropertyDebugName(const char* Prefix, const FPropertyValue& Prop)
	{
		const char* DisplayName = FEditorPropertyRenderer::GetPropertyDisplayName(Prop);
		const char* PropertyName = Prop.GetName();
		const char* Label = DisplayName ? DisplayName : (PropertyName ? PropertyName : "Property");
		return FString(Prefix ? Prefix : "Edit Property")
			+ ": "
			+ FString(Label);
	}

	void AddComponentClassGroup(TArray<FComponentClassGroup>& Groups, const char* Label, UClass* AnchorClass)
	{
		FComponentClassGroup Group;
		Group.Label = Label;
		Group.AnchorClass = AnchorClass;
		Groups.push_back(Group);
	}

	const char* GetPropertyDisplayName(const FPropertyValue& Prop)
	{
		return Prop.GetDisplayName();
	}

	FString TrimActorName(const FString& Name)
	{
		constexpr const char* Whitespace = " \t\r\n";
		const FString::size_type First = Name.find_first_not_of(Whitespace);
		if (First == FString::npos)
		{
			return FString();
		}

		const FString::size_type Last = Name.find_last_not_of(Whitespace);
		return Name.substr(First, Last - First + 1);
	}

	void CopyActorNameToBuffer(AActor* Actor, char* Buffer, size_t BufferSize)
	{
		if (!Actor || !Buffer || BufferSize == 0)
		{
			return;
		}

		strncpy_s(Buffer, BufferSize, Actor->GetFName().ToString().c_str(), _TRUNCATE);
	}

	void CopyObjectNameToBuffer(const UObject* Object, char* Buffer, size_t BufferSize)
	{
		if (!Object || !Buffer || BufferSize == 0)
		{
			return;
		}

		strncpy_s(Buffer, BufferSize, Object->GetFName().ToString().c_str(), _TRUNCATE);
	}

	bool IsActorNameInUse(UWorld* World, AActor* ExcludedActor, const FString& CandidateName)
	{
		if (!World || CandidateName.empty())
		{
			return false;
		}

		const FName CandidateFName(CandidateName);
		for (AActor* Actor : World->GetActors())
		{
			if (Actor && Actor != ExcludedActor && Actor->GetFName() == CandidateFName)
			{
				return true;
			}
		}

		return false;
	}

	bool IsComponentNameInUse(const AActor* OwnerActor, const UActorComponent* ExcludedComponent, const FString& CandidateName)
	{
		if (!OwnerActor || CandidateName.empty())
		{
			return false;
		}

		const FName CandidateFName(CandidateName);
		for (const UActorComponent* Component : OwnerActor->GetComponents())
		{
			if (Component && Component != ExcludedComponent && Component->GetFName() == CandidateFName)
			{
				return true;
			}
		}

		return false;
	}

	void QueueDeferredPostEditChange(TArray<FDeferredPostEditChange>& OutChanges, const FPropertyValue& Prop)
	{
		if (!Prop.Object)
		{
			return;
		}

		FDeferredPostEditChange Change;
		Change.Object = Prop.Object;
		Change.Property = Prop.Property;
		Change.PropertyName = Prop.GetName() ? Prop.GetName() : "";
		Change.DisplayName = GetPropertyDisplayName(Prop) ? GetPropertyDisplayName(Prop) : "";
		Change.Type = Prop.GetType();
		OutChanges.push_back(std::move(Change));
	}

	void FlushDeferredPostEditChanges(TArray<FDeferredPostEditChange>& Changes)
	{
		for (const FDeferredPostEditChange& Change : Changes)
		{
			if (!Change.Object)
			{
				continue;
			}

			FPropertyChangedEvent Event;
			Event.Object = Change.Object;
			Event.Property = Change.Property;
			Event.PropertyName = Change.PropertyName.c_str();
			Event.DisplayName = Change.DisplayName.c_str();
			Event.PropertyPath = Change.PropertyName;
			Event.Type = Change.Type;
			Event.ChangeType = EPropertyChangeType::ValueSet;
			Event.ArrayIndex = -1;
			Change.Object->PostEditChangeProperty(Event);
		}

		Changes.clear();
	}

	bool CopyPropertyValue(const FPropertyValue& SrcValue, FPropertyValue& DstValue)
	{
		void* SrcPtr = SrcValue.GetValuePtr();
		void* DstPtr = DstValue.GetValuePtr();
		if (!SrcPtr || !DstPtr)
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

			DstSoftProperty->SetPath(DstValue.ContainerPtr, SrcSoftProperty->GetPath(SrcValue.ContainerPtr));
			return true;
		}

		if (SrcValue.GetType() != DstValue.GetType())
		{
			return false;
		}

		size_t Size = 0;
		switch (SrcValue.GetType())
		{
		case EPropertyType::Bool:          Size = sizeof(bool); break;
		case EPropertyType::ByteBool:      Size = sizeof(uint8); break;
		case EPropertyType::Int:           Size = sizeof(int32); break;
		case EPropertyType::Float:         Size = sizeof(float); break;
		case EPropertyType::Vec3:
		case EPropertyType::Rotator:       Size = sizeof(float) * 3; break;
		case EPropertyType::Vec4:
		case EPropertyType::Color4:        Size = sizeof(float) * 4; break;
		case EPropertyType::Enum:          Size = SrcValue.GetEnumType() ? SrcValue.GetEnumType()->GetSize() : sizeof(int32); break;
		case EPropertyType::String:
			*static_cast<FString*>(DstPtr) = *static_cast<FString*>(SrcPtr);
			return true;
		case EPropertyType::ObjectRef:
			*static_cast<UObject**>(DstPtr) = *static_cast<UObject**>(SrcPtr);
			return true;
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
		case EPropertyType::Name:
			*static_cast<FName*>(DstPtr) = *static_cast<FName*>(SrcPtr);
			return true;
		case EPropertyType::Array:
		{
			FPropertySerializeContext SrcContext;
			SrcContext.Owner = SrcValue.Object;
			FMemoryArchive Writer(/*bInIsSaving=*/true);
			SrcValue.Property->SerializeValue(SrcPtr, Writer, SrcContext);

			FPropertySerializeContext DstContext;
			DstContext.Owner = DstValue.Object;
			FMemoryArchive Reader(Writer.GetBuffer(), /*bInIsSaving=*/false);
			DstValue.Property->SerializeValue(DstPtr, Reader, DstContext);
			return true;
		}
		case EPropertyType::Struct:
		{
			if (!SrcValue.GetStructType() || !DstValue.GetStructType())
			{
				return false;
			}

			TArray<FPropertyValue> SrcChildren;
			TArray<FPropertyValue> DstChildren;
			SrcValue.GetStructChildren(SrcChildren);
			DstValue.GetStructChildren(DstChildren);

			bool bCopiedAny = false;
			for (const FPropertyValue& SrcChild : SrcChildren)
			{
				for (FPropertyValue& DstChild : DstChildren)
				{
					if (std::strcmp(SrcChild.GetName(), DstChild.GetName()) == 0 && CopyPropertyValue(SrcChild, DstChild))
					{
						bCopiedAny = true;
						break;
					}
				}
			}
			return bCopiedAny;
		}
		default:
			return false;
		}

		if (Size > 0)
		{
			memcpy(DstPtr, SrcPtr, Size);
			return true;
		}

		return false;
	}

	enum class EDetailsTransformPropertyKind
	{
		None,
		Location,
		Rotation,
		Scale
	};

	struct FTransformPropertySnapshot
	{
		bool bValid = false;
		EDetailsTransformPropertyKind Kind = EDetailsTransformPropertyKind::None;
		EPropertyType Type = EPropertyType::Bool;
		FVector VectorValue = FVector::ZeroVector;
		FRotator RotatorValue = FRotator::ZeroRotator;
	};

	struct FTransformPropertyDelta
	{
		bool bValid = false;
		EDetailsTransformPropertyKind Kind = EDetailsTransformPropertyKind::None;
		EPropertyType Type = EPropertyType::Bool;
		FVector VectorDelta = FVector::ZeroVector;
		FRotator RotatorDelta = FRotator::ZeroRotator;
	};

	bool IsCStringEqual(const char* Lhs, const char* Rhs)
	{
		return Lhs && Rhs && std::strcmp(Lhs, Rhs) == 0;
	}

	bool DoesCStringContain(const char* Text, const char* Token)
	{
		return Text && Token && std::strstr(Text, Token) != nullptr;
	}

	EDetailsTransformPropertyKind GetTransformPropertyKind(const FPropertyValue& Prop)
	{
		const char* Category = Prop.GetCategory();
		if (!IsCStringEqual(Category, "Transform"))
		{
			return EDetailsTransformPropertyKind::None;
		}

		const char* DisplayName = GetPropertyDisplayName(Prop);
		const char* PropertyName = Prop.GetName();
		if (IsCStringEqual(DisplayName, "Location") || DoesCStringContain(PropertyName, "Location"))
		{
			return EDetailsTransformPropertyKind::Location;
		}
		if (IsCStringEqual(DisplayName, "Rotation")
			|| DoesCStringContain(PropertyName, "Rotation")
			|| DoesCStringContain(PropertyName, "CachedEditRotator"))
		{
			return EDetailsTransformPropertyKind::Rotation;
		}
		if (IsCStringEqual(DisplayName, "Scale") || DoesCStringContain(PropertyName, "Scale"))
		{
			return EDetailsTransformPropertyKind::Scale;
		}

		return EDetailsTransformPropertyKind::None;
	}

	bool IsTransformDeltaProperty(const FPropertyValue& Prop)
	{
		const EDetailsTransformPropertyKind Kind = GetTransformPropertyKind(Prop);
		if (Kind == EDetailsTransformPropertyKind::None)
		{
			return false;
		}

		if (Kind == EDetailsTransformPropertyKind::Rotation)
		{
			return Prop.GetType() == EPropertyType::Rotator || Prop.GetType() == EPropertyType::Vec3;
		}

		return Prop.GetType() == EPropertyType::Vec3;
	}

	FTransformPropertySnapshot CaptureTransformPropertySnapshot(const FPropertyValue& Prop)
	{
		FTransformPropertySnapshot Snapshot;
		if (!IsTransformDeltaProperty(Prop))
		{
			return Snapshot;
		}

		void* ValuePtr = Prop.GetValuePtr();
		if (!ValuePtr)
		{
			return Snapshot;
		}

		Snapshot.bValid = true;
		Snapshot.Kind = GetTransformPropertyKind(Prop);
		Snapshot.Type = Prop.GetType();
		if (Snapshot.Type == EPropertyType::Rotator)
		{
			Snapshot.RotatorValue = *static_cast<FRotator*>(ValuePtr);
		}
		else
		{
			Snapshot.VectorValue = *static_cast<FVector*>(ValuePtr);
		}

		return Snapshot;
	}

	bool IsTransformDeltaZero(const FTransformPropertyDelta& Delta)
	{
		if (!Delta.bValid)
		{
			return true;
		}

		if (Delta.Type == EPropertyType::Rotator)
		{
			return Delta.RotatorDelta.IsNearlyZero();
		}

		return Delta.VectorDelta.IsNearlyZero();
	}

	FTransformPropertyDelta BuildTransformPropertyDelta(
		const FTransformPropertySnapshot& Before,
		const FPropertyValue& AfterProp)
	{
		FTransformPropertyDelta Delta;
		if (!Before.bValid)
		{
			return Delta;
		}

		const FTransformPropertySnapshot After = CaptureTransformPropertySnapshot(AfterProp);
		if (!After.bValid || After.Kind != Before.Kind || After.Type != Before.Type)
		{
			return Delta;
		}

		Delta.bValid = true;
		Delta.Kind = Before.Kind;
		Delta.Type = Before.Type;
		if (Delta.Type == EPropertyType::Rotator)
		{
			Delta.RotatorDelta = After.RotatorValue - Before.RotatorValue;
		}
		else
		{
			Delta.VectorDelta = After.VectorValue - Before.VectorValue;
		}

		if (IsTransformDeltaZero(Delta))
		{
			Delta.bValid = false;
		}
		return Delta;
	}

	bool IsCompatibleEditableProperty(const FPropertyValue& CandidateProp, const FPropertyValue& SourceProp)
	{
		const char* SourceName = SourceProp.GetName();
		if (!CandidateProp.Property || !CandidateProp.GetName() || !CandidateProp.GetValuePtr() || !SourceName)
		{
			return false;
		}

		if (IsTransformDeltaProperty(SourceProp))
		{
			return IsTransformDeltaProperty(CandidateProp)
				&& GetTransformPropertyKind(CandidateProp) == GetTransformPropertyKind(SourceProp)
				&& CandidateProp.GetType() == SourceProp.GetType();
		}

		return std::strcmp(CandidateProp.GetName(), SourceProp.GetName()) == 0
			&& CandidateProp.GetType() == SourceProp.GetType();
	}

	bool ApplyTransformDeltaToProperty(FPropertyValue& DstProp, const FTransformPropertyDelta& Delta)
	{
		if (!Delta.bValid || !IsTransformDeltaProperty(DstProp))
		{
			return false;
		}

		if (GetTransformPropertyKind(DstProp) != Delta.Kind || DstProp.GetType() != Delta.Type)
		{
			return false;
		}

		void* ValuePtr = DstProp.GetValuePtr();
		if (!ValuePtr)
		{
			return false;
		}

		// 다중 선택 transform 편집은 primary의 절대값이 아니라 사용자가 입력한 변화량만 전파합니다.
		if (Delta.Type == EPropertyType::Rotator)
		{
			*static_cast<FRotator*>(ValuePtr) += Delta.RotatorDelta;
		}
		else
		{
			*static_cast<FVector*>(ValuePtr) += Delta.VectorDelta;
		}

		return true;
	}

	bool HasCompatibleEditableProperty(UActorComponent* Component, const FPropertyValue& SourceProp)
	{
		if (!Component || !SourceProp.Property || !SourceProp.GetName())
		{
			return false;
		}

		TArray<FPropertyValue> Props;
		Component->GetEditableProperties(Props);
		for (const FPropertyValue& Prop : Props)
		{
			if (!Prop.Property || !Prop.GetName())
			{
				continue;
			}

			if (IsCompatibleEditableProperty(Prop, SourceProp))
			{
				return true;
			}
		}

		return false;
	}

	bool HasCompatibleActorEditableProperty(AActor* Actor, const FPropertyValue& SourceProp)
	{
		if (!Actor || !SourceProp.Property || !SourceProp.GetName())
		{
			return false;
		}

		TArray<FPropertyValue> Props;
		Actor->GetEditableProperties(Props);
		for (const FPropertyValue& Prop : Props)
		{
			if (IsCompatibleEditableProperty(Prop, SourceProp))
			{
				return true;
			}
		}

		return false;
	}

	TArray<UObject*> CollectActorPropertyUndoTargets(
		AActor* PrimaryActor,
		const TArray<AActor*>& SelectedActors,
		const FPropertyValue& SourceProp)
	{
		TArray<UObject*> Targets;
		AddUniqueObject(Targets, PrimaryActor);

		if (!PrimaryActor || SelectedActors.size() < 2 || !IsTransformDeltaProperty(SourceProp))
		{
			return Targets;
		}

		for (AActor* Actor : SelectedActors)
		{
			if (!Actor || Actor == PrimaryActor || !Actor->GetRootComponent())
			{
				continue;
			}

			// actor transform은 모든 actor가 공유하는 편집 개념이므로 class/property matching 대신 root transform 기준으로 수집합니다.
			AddUniqueObject(Targets, Actor);
		}

		return Targets;
	}

	TArray<UObject*> CollectComponentPropertyUndoTargets(
		UActorComponent* SelectedComponent,
		const FPropertyValue& SourceProp,
		const TArray<AActor*>& SelectedActors)
	{
		TArray<UObject*> Targets;
		AddUniqueObject(Targets, SelectedComponent);

		if (!SelectedComponent || SelectedActors.size() < 2)
		{
			return Targets;
		}

		UClass* ComponentClass = SelectedComponent->GetClass();
		AActor* PrimaryActor = SelectedActors[0];

		for (AActor* Actor : SelectedActors)
		{
			if (!Actor || Actor == PrimaryActor)
			{
				continue;
			}

			for (UActorComponent* Component : Actor->GetComponents())
			{
				if (!Component || Component->GetClass() != ComponentClass)
				{
					continue;
				}

				// PropagatePropertyChange가 같은 타입의 첫 번째 compatible component에만 값을 전파하므로
				// undo 대상도 동일한 규칙으로 수집합니다.
				if (HasCompatibleEditableProperty(Component, SourceProp))
				{
					AddUniqueObject(Targets, Component);
				}
				break;
			}
		}

		return Targets;
	}

	void PropagatePropertyChange(
		UActorComponent* SelectedComponent,
		const FPropertyValue& SourceProp,
		const FTransformPropertyDelta* TransformDelta,
		const TArray<AActor*>& SelectedActors,
		TArray<FDeferredPostEditChange>& OutDeferredChanges)
	{
		if (!SelectedComponent || SelectedActors.size() < 2) return;

		UClass* CompClass = SelectedComponent->GetClass();
		AActor* PrimaryActor = SelectedActors[0];
		FPropertyValue SrcValue = SourceProp;
		const bool bUseTransformDelta = IsTransformDeltaProperty(SourceProp);
		if (bUseTransformDelta && (!TransformDelta || !TransformDelta->bValid))
		{
			return;
		}

		for (AActor* Actor : SelectedActors)
		{
			if (!Actor || Actor == PrimaryActor) continue;

			for (UActorComponent* Comp : Actor->GetComponents())
			{
				if (!Comp || Comp->GetClass() != CompClass) continue;

				TArray<FPropertyValue> DstProps;
				Comp->GetEditableProperties(DstProps);

				for (FPropertyValue& DstProp : DstProps)
				{
					if (!IsCompatibleEditableProperty(DstProp, SourceProp)) continue;

					bool bApplied = false;
					if (bUseTransformDelta)
					{
						bApplied = ApplyTransformDeltaToProperty(DstProp, *TransformDelta);
					}
					else if (SrcValue.GetValuePtr())
					{
						bApplied = CopyPropertyValue(SrcValue, DstProp);
					}

					if (bApplied)
					{
						QueueDeferredPostEditChange(OutDeferredChanges, DstProp);
					}
					break;
				}
				break; // 같은 타입의 첫 번째 컴포넌트에만 전파
			}
		}
	}

	bool ApplyActorTransformDelta(AActor* Actor, const FTransformPropertyDelta& TransformDelta)
	{
		if (!Actor || !Actor->GetRootComponent() || !TransformDelta.bValid)
		{
			return false;
		}

		// Details의 다중 actor transform 편집은 reflected pending property 대신 actor transform API로 직접 반영합니다.
		// 이렇게 해야 actor class나 root component class와 관계없이 static mesh actor 등도 동일한 delta를 받습니다.
		switch (TransformDelta.Kind)
		{
		case EDetailsTransformPropertyKind::Location:
			Actor->SetActorLocation(Actor->GetActorLocation() + TransformDelta.VectorDelta);
			return true;
		case EDetailsTransformPropertyKind::Rotation:
			if (TransformDelta.Type == EPropertyType::Rotator)
			{
				Actor->SetActorRotation(Actor->GetActorRotation() + TransformDelta.RotatorDelta);
				return true;
			}
			Actor->SetActorRotation(Actor->GetActorRotation() + FRotator(TransformDelta.VectorDelta));
			return true;
		case EDetailsTransformPropertyKind::Scale:
		{
			FVector NewScale = Actor->GetActorScale() + TransformDelta.VectorDelta;
			if (NewScale.X < 0.001f) NewScale.X = 0.001f;
			if (NewScale.Y < 0.001f) NewScale.Y = 0.001f;
			if (NewScale.Z < 0.001f) NewScale.Z = 0.001f;
			Actor->SetActorScale(NewScale);
			return true;
		}
		default:
			return false;
		}
	}

	void PropagateActorTransformPropertyChange(
		AActor* PrimaryActor,
		const FTransformPropertyDelta& TransformDelta,
		const TArray<AActor*>& SelectedActors)
	{
		if (!PrimaryActor || !TransformDelta.bValid || SelectedActors.size() < 2)
		{
			return;
		}

		for (AActor* Actor : SelectedActors)
		{
			if (!Actor || Actor == PrimaryActor)
			{
				continue;
			}

			ApplyActorTransformDelta(Actor, TransformDelta);
		}
	}

	UClass* FindComponentClassGroupAnchor(UClass* ComponentClass, const TArray<FComponentClassGroup>& Groups)
	{
		if (!ComponentClass)
		{
			return nullptr;
		}

		// UTextRenderComponent는 C++ 상속은 Billboard지만 RTTI 등록 부모가 Primitive라서 명시적으로 묶는다.
		if (ComponentClass == UTextRenderComponent::StaticClass())
		{
			return UBillboardComponent::StaticClass();
		}

		for (const FComponentClassGroup& Group : Groups)
		{
			if (Group.AnchorClass && ComponentClass->IsA(Group.AnchorClass))
			{
				return Group.AnchorClass;
			}
		}

		return nullptr;
	}

}

void FEditorPropertyWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	ImGui::SetNextWindowSize(ImVec2(350.0f, 500.0f), ImGuiCond_Once);

	ImGui::Begin("Property Window");

	FSelectionManager& Selection = EditorEngine->GetSelectionManager();
	AActor* PrimaryActor = Selection.GetPrimarySelection();
	if (!PrimaryActor)
	{
		CommitActiveDetailsPropertyUndoIfIdle();
		SelectedComponent = nullptr;
		LastRenameComponent = nullptr;
		LastSelectedActor = nullptr;
		bActorSelected = true;
		ComponentRenameBuffer[0] = '\0';
		ComponentRenameWarning.clear();
		LastObservedActorName = FName();
		LastObservedComponentName = FName();
		ImGui::Text("No object selected.");
		ImGui::End();
		return;
	}

	// Actor 선택이 바뀌면 초기화
	if (PrimaryActor != LastSelectedActor)
	{
		SelectedComponent = nullptr;
		LastRenameComponent = nullptr;
		LastSelectedActor = PrimaryActor;
		bActorSelected = true;
		RenameWarning.clear();
		ComponentRenameBuffer[0] = '\0';
		ComponentRenameWarning.clear();
		CopyActorNameToBuffer(PrimaryActor, RenameBuffer, sizeof(RenameBuffer));
		LastObservedActorName = PrimaryActor->GetFName();
		LastObservedComponentName = FName();
	}
	SyncSelectedComponentAfterStructureChange(PrimaryActor);

	const TArray<AActor*>& SelectedActors = Selection.GetSelectedActors();
	const int32 SelectionCount = static_cast<int32>(SelectedActors.size());

	// ========== 고정 영역: Actor Info (clickable) ==========
	if (SelectionCount > 1)
	{
		ImGui::Text("Class: %s", PrimaryActor->GetClass()->GetName());

		FString PrimaryName = PrimaryActor->GetFName().ToString();
		if (PrimaryName.empty()) PrimaryName = PrimaryActor->GetClass()->GetName();

		bool bHighlight = bActorSelected;
		if (bHighlight) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
		ImGui::Text("Name: %s (+%d)", PrimaryName.c_str(), SelectionCount - 1);
		if (bHighlight) ImGui::PopStyleColor();
		if (ImGui::IsItemClicked())
		{
			bActorSelected = true;
			SelectedComponent = nullptr;
			LastRenameComponent = nullptr;
			ComponentRenameBuffer[0] = '\0';
			ComponentRenameWarning.clear();
		}
		ImGui::SameLine();
		char RemoveLabel[64];
		snprintf(RemoveLabel, sizeof(RemoveLabel), "Remove %d Objects", SelectionCount);
		if (ImGui::Button(RemoveLabel))
		{
			// 삭제 전 진행 중인 속성 트랜잭션을 먼저 닫아 undo 순서를 보존합니다.
			CommitActiveDetailsPropertyUndo();
			EditorEngine->DeleteSelectedActorsWithUndo();
			SelectedComponent = nullptr;
			LastRenameComponent = nullptr;
			ComponentRenameBuffer[0] = '\0';
			ComponentRenameWarning.clear();
			LastSelectedActor = nullptr;
			LastObservedActorName = FName();
			LastObservedComponentName = FName();
			ImGui::End();
			return;
		}
	}
	else
	{
		ImGui::SetWindowFontScale(1.5f);
		ImGui::Text(PrimaryActor->GetFName().ToString().c_str());
		ImGui::SetWindowFontScale(1.0f);

		if (ImGui::IsItemClicked())
		{
			bActorSelected = true;
			SelectedComponent = nullptr;
			LastRenameComponent = nullptr;
			ComponentRenameBuffer[0] = '\0';
			ComponentRenameWarning.clear();
		}
	}

	ImGui::TextUnformatted("Name");
	ImGui::SameLine();
	const float RenameButtonWidth = 72.0f;
	ImGui::SetNextItemWidth(std::max(80.0f, ImGui::GetContentRegionAvail().x - RenameButtonWidth - ImGui::GetStyle().ItemSpacing.x));
	const bool bRenameByEnter = ImGui::InputText("##ActorRename", RenameBuffer, sizeof(RenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
	const bool bRenameByFocusLoss = ImGui::IsItemDeactivatedAfterEdit();
	const bool bActorRenameActive = ImGui::IsItemActive();
	ImGui::SameLine();
	const bool bRenameByButton = ImGui::Button("Rename", ImVec2(RenameButtonWidth, 0.0f));
	if (bRenameByEnter || bRenameByFocusLoss || bRenameByButton)
	{
		RenameActor(PrimaryActor);
	}
	SyncActorRenameBufferIfNeeded(PrimaryActor, bActorRenameActive);

	if (!RenameWarning.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
		ImGui::Text("%s", RenameWarning.c_str());
		ImGui::PopStyleColor();
	}

	// ========== 고정 영역: Component Tree ==========
	RenderComponentTree(PrimaryActor);

	// ========== 스크롤 영역: Details ==========
	float ScrollHeight = ImGui::GetContentRegionAvail().y;
	if (ScrollHeight < 50.0f) ScrollHeight = 50.0f;

	ImGui::BeginChild("##Details", ImVec2(0, ScrollHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
	{
		if (PendingDetailsScrollY >= 0.0f)
		{
			ImGui::SetScrollY(PendingDetailsScrollY);
			PendingDetailsScrollY = -1.0f;
		}

		RenderDetails(PrimaryActor, SelectedActors);
	}
	ImGui::EndChild();

	ImGui::End();
	CommitActiveDetailsPropertyUndoIfIdle();
}

void FEditorPropertyWidget::RenameActor(AActor* PrimaryActor)
{
	if (!PrimaryActor)
	{
		return;
	}

	FString NewName = TrimActorName(FString(RenameBuffer));
	FString CurrentName = PrimaryActor->GetFName().ToString();
	RenameWarning.clear();

	if (NewName.empty())
	{
		RenameWarning = "Actor name cannot be empty.";
		CopyActorNameToBuffer(PrimaryActor, RenameBuffer, sizeof(RenameBuffer));
		return;
	}

	// 현재 이름과 동일하면 스킵
	if (NewName == CurrentName)
	{
		CopyActorNameToBuffer(PrimaryActor, RenameBuffer, sizeof(RenameBuffer));
		return;
	}

	UWorld* World = PrimaryActor->GetWorld();
	if (!World && EditorEngine)
	{
		World = EditorEngine->GetWorld();
	}

	if (IsActorNameInUse(World, PrimaryActor, NewName))
	{
		RenameWarning = "Actor name already exists.";
		return;
	}

	const FName BeforeName = PrimaryActor->GetFName();
	FEditorSelectionSnapshot Selection;
	if (EditorEngine)
	{
		Selection = CaptureEditorSelection(&EditorEngine->GetSelectionManager());
	}
	PrimaryActor->SetFName(FName(NewName));
	if (EditorEngine)
	{
		EditorEngine->PushExecutedUndoCommand(MakeObjectRenameUndoCommand(
			PrimaryActor,
			BeforeName,
			PrimaryActor->GetFName(),
			Selection,
			"Rename Actor"));
	}

	CopyActorNameToBuffer(PrimaryActor, RenameBuffer, sizeof(RenameBuffer));
	LastObservedActorName = PrimaryActor->GetFName();
}

void FEditorPropertyWidget::RenameComponent(AActor* OwnerActor, UActorComponent* Component)
{
	if (!OwnerActor || !Component)
	{
		return;
	}

	FString NewName = TrimActorName(FString(ComponentRenameBuffer));
	FString CurrentName = Component->GetFName().ToString();
	ComponentRenameWarning.clear();

	if (NewName.empty())
	{
		ComponentRenameWarning = "Component name cannot be empty.";
		CopyObjectNameToBuffer(Component, ComponentRenameBuffer, sizeof(ComponentRenameBuffer));
		return;
	}

	if (NewName == CurrentName)
	{
		CopyObjectNameToBuffer(Component, ComponentRenameBuffer, sizeof(ComponentRenameBuffer));
		return;
	}

	if (IsComponentNameInUse(OwnerActor, Component, NewName))
	{
		ComponentRenameWarning = "Component name already exists in this actor.";
		return;
	}

	const FName BeforeName = Component->GetFName();
	FEditorSelectionSnapshot Selection;
	if (EditorEngine)
	{
		Selection = CaptureEditorSelection(&EditorEngine->GetSelectionManager());
	}
	Component->SetFName(FName(NewName));
	if (EditorEngine)
	{
		EditorEngine->PushExecutedUndoCommand(MakeObjectRenameUndoCommand(
			Component,
			BeforeName,
			Component->GetFName(),
			Selection,
			"Rename Component"));
	}

	CopyObjectNameToBuffer(Component, ComponentRenameBuffer, sizeof(ComponentRenameBuffer));
	LastObservedComponentName = Component->GetFName();
}

void FEditorPropertyWidget::RenderDetails(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors)
{
	if (bActorSelected)
	{
		RenderActorProperties(PrimaryActor, SelectedActors);
	}
	else if (SelectedComponent && SelectedActors.size() >= 2)
	{
		// 다중 선택 시 모든 액터의 타입이 동일한지 검증
		UClass* PrimaryClass = PrimaryActor->GetClass();
		bool bAllSameType = true;
		for (const AActor* Actor : SelectedActors)
		{
			if (Actor && Actor->GetClass() != PrimaryClass)
			{
				bAllSameType = false;
				break;
			}
		}

		if (!bAllSameType)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Multi-edit unavailable");
			ImGui::TextWrapped(
				"Selected actors have different types. "
				"Multi-component editing requires all selected actors to be the same type.");

			ImGui::Spacing();
			ImGui::TextDisabled("Primary: %s", PrimaryClass->GetName());
			for (const AActor* Actor : SelectedActors)
			{
				if (Actor && Actor->GetClass() != PrimaryClass)
				{
					ImGui::TextDisabled("  Mismatch: %s (%s)",
						Actor->GetFName().ToString().c_str(),
						Actor->GetClass()->GetName());
				}
			}
		}
		else
		{
			RenderComponentProperties(PrimaryActor, SelectedActors);
		}
	}
	else if (SelectedComponent)
	{
		RenderComponentProperties(PrimaryActor, SelectedActors);
	}
	else
	{
		ImGui::TextDisabled("Select an actor or component to view details.");
	}
}

void FEditorPropertyWidget::FlushPendingDetailsUndoTransaction()
{
	CommitActiveDetailsPropertyUndo();
}

bool FEditorPropertyWidget::DeleteSelectedComponentWithUndo()
{
	if (!EditorEngine || EditorEngine->IsPlayingInEditor())
	{
		return false;
	}

	FSelectionManager& SelectionManager = EditorEngine->GetSelectionManager();
	AActor* Actor = SelectionManager.GetPrimarySelection();
	if (!Actor)
	{
		return false;
	}

	// 뷰포트에서 component를 선택한 직후에도 Details 내부 선택 상태가 같은 대상을 가리키도록 동기화합니다.
	if (USceneComponent* SelectionComponent = SelectionManager.GetSelectedComponent())
	{
		if (SelectionComponent != Actor->GetRootComponent()
			&& DoesActorOwnComponent(Actor, SelectionComponent))
		{
			SelectedComponent = SelectionComponent;
			bActorSelected = false;
			LastSelectedActor = Actor;
		}
	}

	UActorComponent* ComponentToRemove = SelectedComponent;
	if (!ComponentToRemove
		|| ComponentToRemove == Actor->GetRootComponent()
		|| !DoesActorOwnComponent(Actor, ComponentToRemove))
	{
		SyncSelectedComponentAfterStructureChange(Actor);
		return false;
	}

	// 삭제 전 진행 중인 속성 트랜잭션을 먼저 닫아 undo 순서를 보존합니다.
	CommitActiveDetailsPropertyUndo();
	const FEditorSelectionSnapshot SelectionBefore = CaptureEditorSelection(&SelectionManager);
	json::JSON BeforeActorJSON = FSceneSaveManager::SerializeActorForEditorUndo(Actor);
	const FString DebugName = MakeComponentStructureDebugName("Remove Component", ComponentToRemove);

	// 삭제될 component를 selection/gizmo가 물고 있지 않도록 먼저 actor 선택 상태로 되돌립니다.
	SelectionManager.Select(Actor);
	Actor->RemoveComponent(ComponentToRemove);
	SelectedComponent = nullptr;
	LastRenameComponent = nullptr;
	ComponentRenameBuffer[0] = '\0';
	ComponentRenameWarning.clear();
	LastObservedComponentName = FName();
	bActorSelected = true;

	RecordActorStructureUndoChange(
		Actor,
		std::move(BeforeActorJSON),
		SelectionBefore,
		DebugName);
	return true;
}

void FEditorPropertyWidget::RecordDetailsPropertyUndoChange(
	const TArray<FEditorObjectPropertySnapshot>& BeforeSnapshots,
	const TArray<UObject*>& TargetObjects,
	const FString& DebugName)
{
	if (!EditorEngine || BeforeSnapshots.empty() || TargetObjects.empty())
	{
		return;
	}

	const FEditorSelectionSnapshot Selection = CaptureEditorSelection(&EditorEngine->GetSelectionManager());
	const TArray<uint32> TargetUUIDs = MakeObjectUUIDList(TargetObjects);
	if (TargetUUIDs.empty())
	{
		return;
	}

	// 모든 Details 속성 변경은 일단 트랜잭션으로 열어 둡니다.
	// 체크박스/콤보처럼 즉시 끝나는 편집도 PostEditChangeProperty flush 이후에 after 스냅샷을 잡아야
	// PostEditProperty가 함께 수정한 보조 속성까지 undo 스냅샷에 반영됩니다.
	if (!ActivePropertyUndoTransaction.bActive)
	{
		ActivePropertyUndoTransaction.bActive = true;
		ActivePropertyUndoTransaction.TargetObjectUUIDs = TargetUUIDs;
		ActivePropertyUndoTransaction.BeforeSnapshots = BeforeSnapshots;
		ActivePropertyUndoTransaction.Selection = Selection;
		ActivePropertyUndoTransaction.DebugName = DebugName;
		return;
	}

	if (!AreObjectUUIDListsEqual(ActivePropertyUndoTransaction.TargetObjectUUIDs, TargetUUIDs))
	{
		// 예외적으로 다른 객체 편집이 시작되면 기존 트랜잭션을 먼저 닫고 새 대상으로 시작합니다.
		CommitActiveDetailsPropertyUndo();
		ActivePropertyUndoTransaction.bActive = true;
		ActivePropertyUndoTransaction.TargetObjectUUIDs = TargetUUIDs;
		ActivePropertyUndoTransaction.BeforeSnapshots = BeforeSnapshots;
		ActivePropertyUndoTransaction.Selection = Selection;
		ActivePropertyUndoTransaction.DebugName = DebugName;
	}
}

void FEditorPropertyWidget::CommitActiveDetailsPropertyUndo()
{
	if (!EditorEngine || !ActivePropertyUndoTransaction.bActive)
	{
		return;
	}

	// UUID 기준으로 after 스냅샷 대상을 다시 찾으면 undo/redo 적용 중 포인터 교체에도 안전합니다.
	TArray<UObject*> TargetObjects = ResolveObjectsByUUIDs(ActivePropertyUndoTransaction.TargetObjectUUIDs);
	TArray<FEditorObjectPropertySnapshot> AfterSnapshots = CaptureObjectPropertySnapshots(TargetObjects);

	EditorEngine->PushExecutedUndoCommand(MakeObjectPropertyUndoCommand(
		ActivePropertyUndoTransaction.BeforeSnapshots,
		AfterSnapshots,
		ActivePropertyUndoTransaction.Selection,
		ActivePropertyUndoTransaction.DebugName));

	ActivePropertyUndoTransaction = FDetailsPropertyUndoTransaction();
}

void FEditorPropertyWidget::CommitActiveDetailsPropertyUndoIfIdle()
{
	if (ActivePropertyUndoTransaction.bActive && !ImGui::IsAnyItemActive())
	{
		CommitActiveDetailsPropertyUndo();
	}
}

void FEditorPropertyWidget::SyncActorRenameBufferIfNeeded(AActor* Actor, bool bRenameInputActive)
{
	if (!Actor || bRenameInputActive)
	{
		return;
	}

	const FName CurrentName = Actor->GetFName();
	if (CurrentName != LastObservedActorName)
	{
		LastObservedActorName = CurrentName;
		CopyActorNameToBuffer(Actor, RenameBuffer, sizeof(RenameBuffer));
	}
}

void FEditorPropertyWidget::SyncComponentRenameBufferIfNeeded(UActorComponent* Component, bool bRenameInputActive)
{
	if (!Component || bRenameInputActive)
	{
		return;
	}

	const FName CurrentName = Component->GetFName();
	if (CurrentName != LastObservedComponentName)
	{
		LastObservedComponentName = CurrentName;
		CopyObjectNameToBuffer(Component, ComponentRenameBuffer, sizeof(ComponentRenameBuffer));
	}
}

void FEditorPropertyWidget::RecordActorStructureUndoChange(
	AActor* Actor,
	json::JSON BeforeActorJSON,
	const FEditorSelectionSnapshot& SelectionBefore,
	const FString& DebugName)
{
	if (!EditorEngine || !Actor)
	{
		return;
	}

	// 구조 변경 후 actor snapshot과 selection을 함께 기록해 undo/redo 시 Details 선택 상태까지 되돌립니다.
	EditorEngine->PushExecutedUndoCommand(MakeActorStructureUndoCommand(
		Actor,
		std::move(BeforeActorJSON),
		FSceneSaveManager::SerializeActorForEditorUndo(Actor),
		SelectionBefore,
		CaptureEditorSelection(&EditorEngine->GetSelectionManager()),
		DebugName));
}

bool FEditorPropertyWidget::IsSelectedComponentOwnedByActor(AActor* Actor) const
{
	return DoesActorOwnComponent(Actor, SelectedComponent);
}

void FEditorPropertyWidget::SyncSelectedComponentAfterStructureChange(AActor* Actor)
{
	if (!Actor || IsSelectedComponentOwnedByActor(Actor))
	{
		return;
	}

	// undo/redo가 component를 destroy/recreate하면 Details가 들고 있던 raw pointer는 더 이상 유효하지 않습니다.
	// selection manager가 root가 아닌 scene component를 복원했다면 Details도 그 component를 보여주고,
	// 그렇지 않으면 actor Details로 fallback합니다.
	USceneComponent* SelectionComponent = EditorEngine
		? EditorEngine->GetSelectionManager().GetSelectedComponent()
		: nullptr;
	if (SelectionComponent
		&& SelectionComponent != Actor->GetRootComponent()
		&& DoesActorOwnComponent(Actor, SelectionComponent))
	{
		SelectedComponent = SelectionComponent;
		bActorSelected = false;
		LastRenameComponent = nullptr;
		ComponentRenameWarning.clear();
		CopyObjectNameToBuffer(SelectedComponent, ComponentRenameBuffer, sizeof(ComponentRenameBuffer));
		LastObservedComponentName = SelectedComponent->GetFName();
		return;
	}

	SelectedComponent = nullptr;
	bActorSelected = true;
	LastRenameComponent = nullptr;
	ComponentRenameBuffer[0] = '\0';
	ComponentRenameWarning.clear();
	LastObservedComponentName = FName();
}

void FEditorPropertyWidget::RenderActorProperties(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors)
{
	if (PrimaryActor->GetRootComponent())
	{
		ImGui::Separator();
		ImGui::Text("Transform");
		ImGui::Spacing();

		TArray<FPropertyValue> Props;
		PrimaryActor->GetEditableProperties(Props);
		TArray<FDeferredPostEditChange> DeferredChanges;
		bool bAnyChanged = false;

		const float NameColumnWidth = GetDetailsPropertyNameColumnWidth();
		if (ImGui::BeginTable("##ActorPropertyTable", 2, GetDetailsPropertyTableFlags()))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, NameColumnWidth);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

			ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.145f, 0.145f, 0.145f, 1.0f));

			for (int32 i = 0; i < (int32)Props.size(); ++i)
			{
				ImGui::TableNextRow();
				ImGui::PushID(i);

				ImGui::TableSetColumnIndex(0);
				const bool bPropertyOpen = FEditorPropertyRenderer::DrawPropertyLabel(Props[i]);

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-1);

				FEditorPropertyRenderOptions Options;
				Options.bDispatchChange = false;
				Options.bUseExternalExpansion = true;
				Options.bParentExpanded = bPropertyOpen;
				Options.EditedSceneComponent = Cast<USceneComponent>(SelectedComponent);
				TArray<UObject*> UndoTargets = CollectActorPropertyUndoTargets(PrimaryActor, SelectedActors, Props[i]);
				TArray<FEditorObjectPropertySnapshot> UndoBefore;
				bool bUndoBeforeCaptured = false;
				Options.OnBeforePropertyMutate = [&]()
					{
						// 실제 값 변경 직전에만 무거운 JSON snapshot을 생성합니다.
						if (!bUndoBeforeCaptured)
						{
							UndoBefore = CaptureObjectPropertySnapshots(UndoTargets);
							bUndoBeforeCaptured = true;
						}
					};
				const FTransformPropertySnapshot TransformBefore = CaptureTransformPropertySnapshot(Props[i]);
				if (PropertyRenderer.RenderPropertyWidget(Props, i, Options))
				{
					if (!bUndoBeforeCaptured)
					{
						UndoBefore = CaptureObjectPropertySnapshots(UndoTargets);
					}
					const FTransformPropertyDelta TransformDelta = BuildTransformPropertyDelta(TransformBefore, Props[i]);
					bAnyChanged = true;
					QueueDeferredPostEditChange(DeferredChanges, Props[i]);
					PropagateActorTransformPropertyChange(PrimaryActor, TransformDelta, SelectedActors);
					RecordDetailsPropertyUndoChange(
						UndoBefore,
						UndoTargets,
						MakeDetailsPropertyDebugName("Edit Actor Property", Props[i]));
				}
				ImGui::PopID();
			}

			ImGui::EndTable();
			ImGui::PopStyleColor(2);
		}

		if (bAnyChanged)
		{
			PendingDetailsScrollY = ImGui::GetScrollY();
			FlushDeferredPostEditChanges(DeferredChanges);
		}
	}
}

void FEditorPropertyWidget::RenderComponentTree(AActor* Actor)
{
	// Get All Component Classes
	TArray<UClass*>& AllClasses = UClass::GetAllClasses();

	TArray<UClass*> ComponentClasses;
	for (UClass* Cls : AllClasses)
	{
		if (Cls->IsA(UActorComponent::StaticClass()) && !Cls->HasAnyClassFlags(CF_HiddenInComponentList))
			ComponentClasses.push_back(Cls);
	}

	std::sort(ComponentClasses.begin(), ComponentClasses.end(),
		[](const UClass* A, const UClass* B)
		{
			return strcmp(A->GetName(), B->GetName()) < 0;
		});

	//아래 클래스들로 컴포넌트 리스트를 분류합니다.
	TArray<FComponentClassGroup> ComponentGroups;
	AddComponentClassGroup(ComponentGroups, "Light", ULightComponentBase::StaticClass());
	AddComponentClassGroup(ComponentGroups, "Movement", UMovementComponent::StaticClass());
	//AddComponentClassGroup(ComponentGroups, "UBillboardComponent", UBillboardComponent::StaticClass());
	//AddComponentClassGroup(ComponentGroups, "UMeshComponent", UMeshComponent::StaticClass());
	AddComponentClassGroup(ComponentGroups, "Primitive", UPrimitiveComponent::StaticClass());
	//AddComponentClassGroup(ComponentGroups, "USceneComponent", USceneComponent::StaticClass());
	//AddComponentClassGroup(ComponentGroups, "UActorComponent", UActorComponent::StaticClass());

	TArray<UClass*> OtherClasses;
	for (UClass* Cls : ComponentClasses)
	{
		UClass* AnchorClass = FindComponentClassGroupAnchor(Cls, ComponentGroups);
		if (!AnchorClass)
		{
			OtherClasses.push_back(Cls);
			continue;
		}
		for (FComponentClassGroup& Group : ComponentGroups)
		{
			if (Group.AnchorClass == AnchorClass)
			{
				Group.Classes.push_back(Cls);
				break;
			}
		}
	}

	for (FComponentClassGroup& Group : ComponentGroups)
	{
		std::sort(Group.Classes.begin(), Group.Classes.end(),
			[](const UClass* A, const UClass* B)
			{
				return strcmp(A->GetName(), B->GetName()) < 0;
			});
	}
	std::sort(OtherClasses.begin(), OtherClasses.end(),
		[](const UClass* A, const UClass* B)
		{
			return strcmp(A->GetName(), B->GetName()) < 0;
		});

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextUnformatted("Components");
	ImGui::SameLine();

	if (ImGui::Button("Add"))
	{
		ImGui::OpenPopup("##AddComponentPopup");
	}

	if (ImGui::BeginPopup("##AddComponentPopup"))
	{
		auto AddComponentClassItem = [&](UClass* Cls)
		{
			if (ImGui::Selectable(Cls->GetName()))
			{
				AddComponentToActor(Actor, Cls);
				ImGui::CloseCurrentPopup();
			}
		};

		for (const FComponentClassGroup& Group : ComponentGroups)
		{
			if (Group.Classes.empty()) continue;

			if (ImGui::TreeNode(Group.Label))
			{
				for (UClass* Cls : Group.Classes)
				{
					AddComponentClassItem(Cls);
				}

				ImGui::TreePop();
			}
		}

		if (!OtherClasses.empty())
		{
			if (ImGui::TreeNode("Other"))
			{
				for (UClass* Cls : OtherClasses)
				{
					AddComponentClassItem(Cls);
				}

				ImGui::TreePop();
			}
		}

		ImGui::EndPopup();
	}

	ImGui::Separator();

	USceneComponent* Root = Actor->GetRootComponent();

	static float TreeHeight = 100.0f;

	ImGui::BeginChild("##ComponentTree", ImVec2(0, TreeHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
	{
		if (Root)
		{
			RenderSceneComponentNode(Root);
		}

		TArray<UActorComponent*> NonSceneComponents;
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (!Comp) continue;
			if (Comp->IsA<USceneComponent>()) continue;
			if (ShouldHideInComponentTree(Comp, bShowEditorOnlyComponents)) continue;
			NonSceneComponents.push_back(Comp);
		}

		if (!NonSceneComponents.empty())
		{
			ImGui::Separator();
		}

		for (UActorComponent* Comp : NonSceneComponents)
		{
			FString Name = Comp->GetFName().ToString();
			const FString TypeName = Comp->GetClass()->GetName();
			const FString DefaultNamePrefix = TypeName + "_";

			const bool bUseTypeAsLabel = Name.empty() || Name == TypeName || Name.rfind(DefaultNamePrefix, 0) == 0;

			const char* Label = bUseTypeAsLabel ? TypeName.c_str() : Name.c_str();

			ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

			if (!bActorSelected && SelectedComponent == Comp)
			{
				Flags |= ImGuiTreeNodeFlags_Selected;
			}

			ImGui::TreeNodeEx(Comp, Flags, "%s", Label);
		
			if (ImGui::IsItemClicked())
			{
				SelectedComponent = Comp;
				bActorSelected = false;
			}
		}
	}

	ImGui::EndChild();

	ImGui::InvisibleButton("##TreeResize", ImVec2(-1, 6));

	if (ImGui::IsItemHovered() || ImGui::IsItemActive())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
	}

	if (ImGui::IsItemActive())
	{
		TreeHeight += ImGui::GetIO().MouseDelta.y;
		TreeHeight = std::max(TreeHeight, 80.0f);
	}

	ImVec2 Min = ImGui::GetItemRectMin();
	ImVec2 Max = ImGui::GetItemRectMax();

	ImU32 Color =
		ImGui::GetColorU32(
			ImGui::IsItemHovered()
			? ImGuiCol_SeparatorHovered
			: ImGuiCol_Separator
		);

	ImGui::GetWindowDrawList()->AddLine(
		ImVec2(Min.x, (Min.y + Max.y) * 0.5f),
		ImVec2(Max.x, (Min.y + Max.y) * 0.5f),
		Color,
		2.0f
	);
}

void FEditorPropertyWidget::RenderSceneComponentNode(USceneComponent* Comp)
{
	if (!Comp) return;
	if (ShouldHideInComponentTree(Comp, bShowEditorOnlyComponents)) return;

	FString Name = Comp->GetFName().ToString();
	if (Name.empty()) Name = Comp->GetClass()->GetName();

	const auto& Children = Comp->GetChildren();
	bool bHasVisibleChildren = false;
	for (USceneComponent* Child : Children)
	{
		if (Child && !ShouldHideInComponentTree(Child, bShowEditorOnlyComponents))
		{
			bHasVisibleChildren = true;
			break;
		}
	}

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
	if (!bHasVisibleChildren)
		Flags |= ImGuiTreeNodeFlags_Leaf;
	if (!bActorSelected && SelectedComponent == Comp)
		Flags |= ImGuiTreeNodeFlags_Selected;

	bool bIsRoot = (Comp->GetParent() == nullptr);
	bool bOpen = ImGui::TreeNodeEx(
		Comp, Flags, "%s%s (%s)",
		bIsRoot ? "[Root] " : "",
		Name.c_str(),
		Comp->GetClass()->GetName()
	);

	if (ImGui::IsItemClicked())
	{
		SelectedComponent = Comp;
		bActorSelected = false;
		EditorEngine->GetSelectionManager().SelectComponent(Comp);
	}

	// 컴포넌트 트리에서 간단하게 드래그 앤 드랍으로 부모-자식 관계 변경 가능하도록 지원
	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("SCENE_COMPONENT_REPARENT", &Comp, sizeof(USceneComponent*));
		ImGui::Text("Reparent %s", Name.c_str());
		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_COMPONENT_REPARENT"))
		{
			USceneComponent* DraggedComp = *(USceneComponent**)payload->Data;
			if (DraggedComp && DraggedComp != Comp)
			{
				// Circular dependency check: Ensure Comp is not a child of DraggedComp
				bool bIsChildOfDragged = false;
				USceneComponent* Check = Comp;
				while (Check)
				{
					if (Check == DraggedComp)
					{
						bIsChildOfDragged = true;
						break;
					}
					Check = Check->GetParent();
				}

				if (!bIsChildOfDragged)
				{
					CommitActiveDetailsPropertyUndo();
					AActor* OwnerActor = Comp->GetOwner();
					const FEditorSelectionSnapshot SelectionBefore = CaptureEditorSelection(&EditorEngine->GetSelectionManager());
					json::JSON BeforeActorJSON = FSceneSaveManager::SerializeActorForEditorUndo(OwnerActor);
					const FString DebugName = MakeComponentStructureDebugName("Reparent Component", DraggedComp);

					DraggedComp->SetParent(Comp);
					if (EditorEngine && EditorEngine->GetGizmo())
					{
						EditorEngine->GetGizmo()->UpdateGizmoTransform();
					}

					RecordActorStructureUndoChange(
						OwnerActor,
						std::move(BeforeActorJSON),
						SelectionBefore,
						DebugName);
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	if (bOpen)
	{
		for (USceneComponent* Child : Children)
		{
			RenderSceneComponentNode(Child);
		}
		ImGui::TreePop();
	}
}

void FEditorPropertyWidget::RenderComponentProperties(AActor* Actor, const TArray<AActor*>& SelectedActors)
{
	if (!Actor || !SelectedComponent)
	{
		return;
	}

	if (SelectedComponent != LastRenameComponent)
	{
		LastRenameComponent = SelectedComponent;
		ComponentRenameWarning.clear();
		CopyObjectNameToBuffer(SelectedComponent, ComponentRenameBuffer, sizeof(ComponentRenameBuffer));
		LastObservedComponentName = SelectedComponent->GetFName();
	}

	ImGui::TextUnformatted("Name");
	ImGui::SameLine();
	const float RenameButtonWidth = 72.0f;
	ImGui::SetNextItemWidth(std::max(80.0f, ImGui::GetContentRegionAvail().x - RenameButtonWidth - ImGui::GetStyle().ItemSpacing.x));
	const bool bRenameByEnter = ImGui::InputText("##ComponentRename", ComponentRenameBuffer,
		sizeof(ComponentRenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
	const bool bRenameByFocusLoss = ImGui::IsItemDeactivatedAfterEdit();
	const bool bComponentRenameActive = ImGui::IsItemActive();
	ImGui::SameLine();
	const bool bRenameByButton = ImGui::Button("Rename##Component", ImVec2(RenameButtonWidth, 0.0f));
	if (bRenameByEnter || bRenameByFocusLoss || bRenameByButton)
	{
		RenameComponent(Actor, SelectedComponent);
	}
	SyncComponentRenameBufferIfNeeded(SelectedComponent, bComponentRenameActive);

	if (!ComponentRenameWarning.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
		ImGui::Text("%s", ComponentRenameWarning.c_str());
		ImGui::PopStyleColor();
	}

	if (SelectedComponent != Actor->GetRootComponent())
	{
		if (ImGui::Button("Remove"))
		{
			DeleteSelectedComponentWithUndo();
			return;
		}
	}

	ImGui::Separator();

	// reflected property 기반 자동 위젯 렌더링
	TArray<FPropertyValue> Props;
	SelectedComponent->GetEditableProperties(Props);

	bool bIsRoot = false;
	if (SelectedComponent->IsA<USceneComponent>())
	{
		USceneComponent* SceneComp = static_cast<USceneComponent*>(SelectedComponent);
		bIsRoot = (SceneComp->GetParent() == nullptr);
	}

	// 카테고리 순서 수집 (등장 순 유지)
	TArray<std::string> CategoryOrder;
	for (const auto& P : Props)
	{
		const char* PropertyCategory = P.GetCategory();
		bool bFound = false;
		for (const auto& C : CategoryOrder)
		{
			if (C == PropertyCategory) { bFound = true; break; }
		}
		if (!bFound) CategoryOrder.push_back(PropertyCategory);
	}

	bool bAnyChanged = false;
	TArray<FDeferredPostEditChange> DeferredChanges;

	for (const auto& Cat : CategoryOrder)
	{
		// Root 컴포넌트는 Transform 카테고리 스킵
		if (bIsRoot && Cat == "Transform")
			continue;

		// 카테고리 헤더 (빈 문자열이면 헤더 없이 렌더)
		bool bInTreeNode = false;
		if (!Cat.empty())
		{
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.27f, 0.27f, 0.27f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.30f, 0.30f, 0.30f, 1.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 3.0f));

			bool bOpen = ImGui::CollapsingHeader(Cat.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

			ImGui::PopStyleVar();
			ImGui::PopStyleColor(3);

			if (!bOpen) continue;
		}

		const float NameColumnWidth = GetDetailsPropertyNameColumnWidth();
		if (ImGui::BeginTable("##PropertyTable", 2, GetDetailsPropertyTableFlags()))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, NameColumnWidth);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

			ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.145f, 0.145f, 0.145f, 1.0f));

			for (int32 i = 0; i < (int32)Props.size(); ++i)
			{
				if (Cat != Props[i].GetCategory())
					continue;

				ImGui::TableNextRow();
				ImGui::PushID(i);

				ImGui::TableSetColumnIndex(0);
				const bool bPropertyOpen = FEditorPropertyRenderer::DrawPropertyLabel(Props[i]);

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-1);

				FEditorPropertyRenderOptions Options;
				Options.bDispatchChange = false;
				Options.bUseExternalExpansion = true;
				Options.bParentExpanded = bPropertyOpen;
				Options.EditedSceneComponent = Cast<USceneComponent>(SelectedComponent);
				TArray<UObject*> UndoTargets = CollectComponentPropertyUndoTargets(SelectedComponent, Props[i], SelectedActors);
				TArray<FEditorObjectPropertySnapshot> UndoBefore;
				bool bUndoBeforeCaptured = false;
				Options.OnBeforePropertyMutate = [&]()
					{
						// 실제 값 변경 직전에만 무거운 JSON snapshot을 생성합니다.
						if (!bUndoBeforeCaptured)
						{
							UndoBefore = CaptureObjectPropertySnapshots(UndoTargets);
							bUndoBeforeCaptured = true;
						}
					};
				const FTransformPropertySnapshot TransformBefore = CaptureTransformPropertySnapshot(Props[i]);
				bool bChanged = PropertyRenderer.RenderPropertyWidget(Props, i, Options);

				if (bChanged)
				{
					if (!bUndoBeforeCaptured)
					{
						UndoBefore = CaptureObjectPropertySnapshots(UndoTargets);
					}
					const FTransformPropertyDelta TransformDelta = BuildTransformPropertyDelta(TransformBefore, Props[i]);
					bAnyChanged = true;
					QueueDeferredPostEditChange(DeferredChanges, Props[i]);
					PropagatePropertyChange(
						SelectedComponent,
						Props[i],
						TransformDelta.bValid ? &TransformDelta : nullptr,
						SelectedActors,
						DeferredChanges);
					RecordDetailsPropertyUndoChange(
						UndoBefore,
						UndoTargets,
						MakeDetailsPropertyDebugName("Edit Component Property", Props[i]));
				}
				ImGui::PopID();
			}

			ImGui::EndTable();
			ImGui::PopStyleColor(2);
		}
	}

	if (bAnyChanged)
	{
		PendingDetailsScrollY = ImGui::GetScrollY();
		FlushDeferredPostEditChanges(DeferredChanges);
	}

	// 실제 변경이 있었을 때만 Transform dirty 마킹
	if (bAnyChanged && SelectedComponent && SelectedComponent->IsA<USceneComponent>())
	{
		static_cast<USceneComponent*>(SelectedComponent)->MarkTransformDirty();
	}
}

void FEditorPropertyWidget::AddComponentToActor(AActor* Actor, UClass* ComponentClass)
{
	if (!Actor || !ComponentClass) return;

	// component 구조 변경 전에 진행 중인 속성 transaction을 먼저 닫아 undo 순서를 보존합니다.
	CommitActiveDetailsPropertyUndo();
	const FEditorSelectionSnapshot SelectionBefore = CaptureEditorSelection(&EditorEngine->GetSelectionManager());
	json::JSON BeforeActorJSON = FSceneSaveManager::SerializeActorForEditorUndo(Actor);

	UActorComponent* Comp = Actor->AddComponentByClass(ComponentClass);
	if (!Comp) return;

	if (ComponentClass->IsA(USceneComponent::StaticClass()))
	{
		USceneComponent* Root = Actor->GetRootComponent();
		USceneComponent* SceneComp = Cast<USceneComponent>(Comp);
		USceneComponent* AttachParent = DoesActorOwnComponent(Actor, SelectedComponent)
			? Cast<USceneComponent>(SelectedComponent)
			: nullptr;

		if (AttachParent)
		{
			SceneComp->AttachToComponent(AttachParent);
		}
		else
		{
			SceneComp->AttachToComponent(Root);
		}

		if (Comp->IsA<ULightComponentBase>())
		{
			Cast<ULightComponentBase>(Comp)->EnsureEditorBillboard();
		}
		else if (Comp->IsA<UDecalComponent>())
		{
			Cast<UDecalComponent>(Comp)->EnsureEditorBillboard();
		}
		else if (Comp->IsA<UHeightFogComponent>())
		{
			Cast<UHeightFogComponent>(Comp)->EnsureEditorBillboard();
		}

		if (SceneComp)
		{
			EditorEngine->GetSelectionManager().SelectComponent(SceneComp);
		}
	}
	else
	{
		EditorEngine->GetSelectionManager().Select(Actor);
	}

	SelectedComponent = Comp;
	bActorSelected = false;
	LastRenameComponent = nullptr;
	ComponentRenameWarning.clear();
	CopyObjectNameToBuffer(SelectedComponent, ComponentRenameBuffer, sizeof(ComponentRenameBuffer));
	LastObservedComponentName = SelectedComponent->GetFName();

	RecordActorStructureUndoChange(
		Actor,
		std::move(BeforeActorJSON),
		SelectionBefore,
		MakeComponentStructureDebugName("Add Component", Comp));
}
