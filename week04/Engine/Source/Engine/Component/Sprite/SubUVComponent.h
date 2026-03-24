#pragma once

#include "AtlasComponent.h"

namespace Engine::Component
{
    class ENGINE_API USubUVComponent : public UAtlasComponent
    {
        DECLARE_RTTI(USubUVComponent, UAtlasComponent)
      public:
        USubUVComponent() = default;
        ~USubUVComponent() override = default;

        int32 GetFrameIndex() const { return FrameIndex; }
        void  SetFrameIndex(int32 InFrameIndex);

        int32 GetFrameCount() const;

      protected:
        int32 FrameIndex = 0;
    };
} // namespace Engine::Component
