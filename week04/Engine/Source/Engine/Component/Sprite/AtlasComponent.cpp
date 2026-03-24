#include "AtlasComponent.h"

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

    REGISTER_CLASS(Engine::Component, UAtlasComponent)
} // namespace Engine::Component
