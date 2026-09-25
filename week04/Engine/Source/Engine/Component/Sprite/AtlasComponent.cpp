#include "AtlasComponent.h"
#include "Engine/Component/Core/ComponentProperty.h"

namespace Engine::Component
{
    void UAtlasComponent::SetAtlasRows(int32 InAtlasRows)
    {
        AtlasRows = std::max(1, InAtlasRows);
    }

    void UAtlasComponent::SetAtlasColumns(int32 InAtlasColumns)
    {
        AtlasColumns = std::max(1, InAtlasColumns);
    }

    void UAtlasComponent::SetAtlasGrid(int32 InAtlasRows, int32 InAtlasColumns)
    {
        SetAtlasRows(InAtlasRows);
        SetAtlasColumns(InAtlasColumns);
    }

    void UAtlasComponent::DescribeProperties(FComponentPropertyBuilder& Builder)
    {
        USpriteComponent::DescribeProperties(Builder); // 부모 호출 변경

        Builder.AddInt(
            "atlas_rows", L"Atlas Rows", [this]() { return GetAtlasRows(); },
            [this](int32 InValue) { SetAtlasRows(InValue); });

        Builder.AddInt(
            "atlas_columns", L"Atlas Columns", [this]() { return GetAtlasColumns(); },
            [this](int32 InValue) { SetAtlasColumns(InValue); });
    }

    REGISTER_CLASS(Engine::Component, UAtlasComponent)
} // namespace Engine::Component
