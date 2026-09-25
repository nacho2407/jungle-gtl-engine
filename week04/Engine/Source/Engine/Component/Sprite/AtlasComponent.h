#pragma once

#include "Engine/Component/Sprite/SpriteComponent.h"

namespace Engine::Component
{
    /**
     * @brief 아틀라스(Grid) 기반 스프라이트 컴포넌트입니다.
     */
    class ENGINE_API UAtlasComponent : public USpriteComponent
    {
        DECLARE_RTTI(UAtlasComponent, USpriteComponent)
      public:
        UAtlasComponent() = default;
        ~UAtlasComponent() override = default;

        int32 GetAtlasRows() const { return AtlasRows; }
        int32 GetAtlasColumns() const { return AtlasColumns; }

        void SetAtlasRows(int32 InAtlasRows);
        void SetAtlasColumns(int32 InAtlasColumns);
        void SetAtlasGrid(int32 InAtlasRows, int32 InAtlasColumns);
        
        void DescribeProperties(class FComponentPropertyBuilder& Builder) override;

      protected:
        int32 AtlasRows = 1;
        int32 AtlasColumns = 1;
    };
} // namespace Engine::Component
