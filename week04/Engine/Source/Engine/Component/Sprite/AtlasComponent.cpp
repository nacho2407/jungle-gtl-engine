#include "AtlasComponent.h"

#include "Engine/Component/Core/ComponentProperty.h"

namespace Engine::Component
{
    void UAtlasComponent::SetAtlasRows(int32 InAtlasRows)
    {
        AtlasRows = (InAtlasRows > 0) ? InAtlasRows : 1;
    }

    void UAtlasComponent::SetAtlasColumns(int32 InAtlasColumns)
    {
        AtlasColumns = (InAtlasColumns > 0) ? InAtlasColumns : 1;
    }

    void UAtlasComponent::SetAtlasGrid(int32 InAtlasRows, int32 InAtlasColumns)
    {
        SetAtlasRows(InAtlasRows);
        SetAtlasColumns(InAtlasColumns);
    }

    void UAtlasComponent::DescribeProperties(FComponentPropertyBuilder& Builder)
    {
        UQuadComponent::DescribeProperties(Builder);

        FComponentPropertyOptions IntOptions;
        IntOptions.DragSpeed = 1.0f;

        Builder.AddInt(
            "atlas_rows", L"Atlas Rows", [this]() { return GetAtlasRows(); },
            [this](int32 InValue) { SetAtlasRows(InValue); }, IntOptions);
        Builder.AddInt(
            "atlas_columns", L"Atlas Columns", [this]() { return GetAtlasColumns(); },
            [this](int32 InValue) { SetAtlasColumns(InValue); }, IntOptions);
    }

    REGISTER_CLASS(Engine::Component, UAtlasComponent)
} // namespace Engine::Component
