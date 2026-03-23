#pragma once

#include "Engine/Component/Mesh/QuadComponent.h"

namespace Engine::Component
{
    class ENGINE_API UAtlasComponent : public UQuadComponent
    {
        DECLARE_RTTI(UAtlasComponent, UQuadComponent)
      public:
        UAtlasComponent() = default;
        ~UAtlasComponent() override = default;

        int32 GetAtlasRows() const { return AtlasRows; }
        int32 GetAtlasColumns() const { return AtlasColumns; }

        void SetAtlasRows(int32 InAtlasRows);
        void SetAtlasColumns(int32 InAtlasColumns);
        void SetAtlasGrid(int32 InAtlasRows, int32 InAtlasColumns);

      protected:
        int32 AtlasRows = 1;
        int32 AtlasColumns = 1;
    };
} // namespace Engine::Component
