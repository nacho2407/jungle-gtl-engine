#pragma once

#include <cassert>
#include <cstddef>
#include <type_traits>

#include "BoxElem.h"
#include "SphereElem.h"
#include "SphylElem.h"
#include "ConvexElem.h"

#include "Source/Engine/PhysicsEngine/AggregateGeom.generated.h"

USTRUCT()
struct FKAggregateGeom
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Aggregate Geometry", DisplayName="Sphere", Type=Array)
	TArray<FKSphereElem> SphereElems;

	UPROPERTY(Edit, Save, Category="Aggregate Geometry", DisplayName="Box", Type=Array)
	TArray<FKBoxElem> BoxElems;

	UPROPERTY(Edit, Save, Category="Aggregate Geometry", DisplayName="Capsule", Type=Array)
	TArray<FKSphylElem> SphylElems;

	UPROPERTY(Edit, Save, Category="Aggregate Geometry", DisplayName="Convex", Type=Array)
	TArray<FKConvexElem> ConvexElems;

	FKAggregateGeom() = default;

	FKAggregateGeom(const FKAggregateGeom& Other)
	{
		CloneAgg(Other);
	}

	const FKAggregateGeom& operator=(const FKAggregateGeom& Other)
	{
		CloneAgg(Other);
		return *this;
	}

	int32 GetElementCount() const
	{
		return static_cast<int32>(SphereElems.size() + SphylElems.size() + BoxElems.size() + ConvexElems.size());
	}

	int32 GetElementCount(EAggCollisionShape Type) const
	{
		switch (Type)
		{
		case EAggCollisionShape::Sphere:
			return static_cast<int32>(SphereElems.size());
		case EAggCollisionShape::Box:
			return static_cast<int32>(BoxElems.size());
		case EAggCollisionShape::Sphyl:
			return static_cast<int32>(SphylElems.size());
		case EAggCollisionShape::Convex:
			return static_cast<int32>(ConvexElems.size());
		default:
			assert(false);
			return 0;
		}
	}

	template <typename Callable>
	auto VisitShapeAndContainer(FKShapeElem& InElement, Callable&& InCallable)
	{
		switch (InElement.GetShapeType())
		{
		case EAggCollisionShape::Sphere:
			return InCallable(static_cast<FKSphereElem&>(InElement), SphereElems);
		case EAggCollisionShape::Box:
			return InCallable(static_cast<FKBoxElem&>(InElement), BoxElems);
		case EAggCollisionShape::Sphyl:
			return InCallable(static_cast<FKSphylElem&>(InElement), SphylElems);
		case EAggCollisionShape::Convex:
			return InCallable(static_cast<FKConvexElem&>(InElement), ConvexElems);
		default:
			assert(false);
		}

		using RetType = std::invoke_result_t<Callable, FKSphereElem&, TArray<FKSphereElem>&>;
		if constexpr (!std::is_same_v<RetType, void>)
		{
			return RetType{};
		}
	}

	template <typename Callable>
	auto VisitShapeAndContainer(const FKShapeElem& InElement, Callable&& InCallable) const
	{
		switch (InElement.GetShapeType())
		{
		case EAggCollisionShape::Sphere:
			return InCallable(static_cast<const FKSphereElem&>(InElement), SphereElems);
		case EAggCollisionShape::Box:
			return InCallable(static_cast<const FKBoxElem&>(InElement), BoxElems);
		case EAggCollisionShape::Sphyl:
			return InCallable(static_cast<const FKSphylElem&>(InElement), SphylElems);
		case EAggCollisionShape::Convex:
			return InCallable(static_cast<const FKConvexElem&>(InElement), ConvexElems);
		default:
			assert(false);
		}

		using RetType = std::invoke_result_t<Callable, const FKSphereElem&, const TArray<FKSphereElem>&>;
		if constexpr (!std::is_same_v<RetType, void>)
		{
			return RetType{};
		}
	}

	template <typename T>
	void AddElement(const T& Elem)
	{
		static_assert(std::is_base_of_v<FKShapeElem, T>, "T must derive from FKShapeElem");

		switch (Elem.GetShapeType())
		{
		case EAggCollisionShape::Sphere:
			SphereElems.push_back(static_cast<const FKSphereElem&>(Elem));
			break;
		case EAggCollisionShape::Box:
			BoxElems.push_back(static_cast<const FKBoxElem&>(Elem));
			break;
		case EAggCollisionShape::Sphyl:
			SphylElems.push_back(static_cast<const FKSphylElem&>(Elem));
			break;
		case EAggCollisionShape::Convex:
			ConvexElems.push_back(static_cast<const FKConvexElem&>(Elem));
			break;
		default:
			assert(false);
			break;
		}
	}

	FKShapeElem* GetElement(const EAggCollisionShape Type, const int32 Index)
	{
		switch (Type)
		{
		case EAggCollisionShape::Sphere:
			return GetElementIfValid(SphereElems, Index);
		case EAggCollisionShape::Box:
			return GetElementIfValid(BoxElems, Index);
		case EAggCollisionShape::Sphyl:
			return GetElementIfValid(SphylElems, Index);
		case EAggCollisionShape::Convex:
			return GetElementIfValid(ConvexElems, Index);
		default:
			assert(false);
			return nullptr;
		}
	}

	const FKShapeElem* GetElement(const EAggCollisionShape Type, const int32 Index) const
	{
		switch (Type)
		{
		case EAggCollisionShape::Sphere:
			return GetElementIfValid(SphereElems, Index);
		case EAggCollisionShape::Box:
			return GetElementIfValid(BoxElems, Index);
		case EAggCollisionShape::Sphyl:
			return GetElementIfValid(SphylElems, Index);
		case EAggCollisionShape::Convex:
			return GetElementIfValid(ConvexElems, Index);
		default:
			assert(false);
			return nullptr;
		}
	}

	FKShapeElem* GetElement(const int32 InIndex)
	{
		int32 Index = InIndex;
		if (FKShapeElem* Element = GetElementFromFlatIndex(SphereElems, Index)) return Element;
		if (FKShapeElem* Element = GetElementFromFlatIndex(BoxElems, Index)) return Element;
		if (FKShapeElem* Element = GetElementFromFlatIndex(SphylElems, Index)) return Element;
		if (FKShapeElem* Element = GetElementFromFlatIndex(ConvexElems, Index)) return Element;

		assert(false);
		return nullptr;
	}

	const FKShapeElem* GetElement(const int32 InIndex) const
	{
		int32 Index = InIndex;
		if (const FKShapeElem* Element = GetElementFromFlatIndex(SphereElems, Index)) return Element;
		if (const FKShapeElem* Element = GetElementFromFlatIndex(BoxElems, Index)) return Element;
		if (const FKShapeElem* Element = GetElementFromFlatIndex(SphylElems, Index)) return Element;
		if (const FKShapeElem* Element = GetElementFromFlatIndex(ConvexElems, Index)) return Element;

		assert(false);
		return nullptr;
	}

	const FKShapeElem* GetElementByName(const FName InName) const
	{
		if (const FKShapeElem* FoundSphereElem = GetElementByName(SphereElems, InName)) return FoundSphereElem;
		if (const FKShapeElem* FoundBoxElem = GetElementByName(BoxElems, InName)) return FoundBoxElem;
		if (const FKShapeElem* FoundSphylElem = GetElementByName(SphylElems, InName)) return FoundSphylElem;
		if (const FKShapeElem* FoundConvexElem = GetElementByName(ConvexElems, InName)) return FoundConvexElem;

		return nullptr;
	}

	int32 GetElementIndexByName(const FName InName) const
	{
		int32 StartIndex = 0;

		if (const int32 FoundIndex = GetElementIndexByName(SphereElems, InName); FoundIndex != InvalidIndex)
		{
			return FoundIndex + StartIndex;
		}
		StartIndex += static_cast<int32>(SphereElems.size());

		if (const int32 FoundIndex = GetElementIndexByName(BoxElems, InName); FoundIndex != InvalidIndex)
		{
			return FoundIndex + StartIndex;
		}
		StartIndex += static_cast<int32>(BoxElems.size());

		if (const int32 FoundIndex = GetElementIndexByName(SphylElems, InName); FoundIndex != InvalidIndex)
		{
			return FoundIndex + StartIndex;
		}
		StartIndex += static_cast<int32>(SphylElems.size());

		if (const int32 FoundIndex = GetElementIndexByName(ConvexElems, InName); FoundIndex != InvalidIndex)
		{
			return FoundIndex + StartIndex;
		}

		return InvalidIndex;
	}

	void EmptyElements()
	{
		BoxElems.clear();
		ConvexElems.clear();
		SphylElems.clear();
		SphereElems.clear();
	}

	FBoundingBox CalcAABB(const FTransform& Transform) const;
	float GetScaledVolume(const FVector& Scale3D = FVector::OneVector) const;

private:
	static constexpr int32 InvalidIndex = -1;

	template <typename ContainerType>
	static bool IsValidContainerIndex(const ContainerType& Elements, const int32 Index)
	{
		return Index >= 0 && static_cast<std::size_t>(Index) < Elements.size();
	}

	template <typename ContainerType>
	static auto GetElementIfValid(ContainerType& Elements, const int32 Index) -> decltype(Elements.data())
	{
		if (IsValidContainerIndex(Elements, Index))
		{
			return Elements.data() + Index;
		}

		assert(false);
		return nullptr;
	}

	template <typename ContainerType>
	static auto GetElementFromFlatIndex(ContainerType& Elements, int32& Index) -> decltype(Elements.data())
	{
		if (IsValidContainerIndex(Elements, Index))
		{
			return Elements.data() + Index;
		}

		Index -= static_cast<int32>(Elements.size());
		return nullptr;
	}

	void CloneAgg(const FKAggregateGeom& Other)
	{
		SphereElems = Other.SphereElems;
		BoxElems = Other.BoxElems;
		SphylElems = Other.SphylElems;
		ConvexElems = Other.ConvexElems;
	}

	template <class T>
	const FKShapeElem* GetElementByName(const TArray<T>& Elements, const FName& InName) const
	{
		for (const T& Elem : Elements)
		{
			if (Elem.GetName() == InName)
			{
				return &Elem;
			}
		}

		return nullptr;
	}

	template <class T>
	int32 GetElementIndexByName(const TArray<T>& Elements, const FName& InName) const
	{
		for (std::size_t Index = 0; Index < Elements.size(); ++Index)
		{
			if (Elements[Index].GetName() == InName)
			{
				return static_cast<int32>(Index);
			}
		}

		return InvalidIndex;
	}
};
