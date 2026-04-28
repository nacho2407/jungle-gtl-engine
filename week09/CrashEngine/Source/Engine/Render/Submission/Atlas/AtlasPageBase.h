#pragma once

#include "Core/CoreTypes.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"

// shadow/decal 같은 atlas page가 공통으로 상속받는 베이스입니다.
class FAtlasPageBase
{
public:
    FAtlasPageBase(uint32 InAtlasSize, uint32 InSliceCount)
        : AtlasSize(InAtlasSize)
        , SliceCount(InSliceCount)
    {
    }

    virtual ~FAtlasPageBase() = default;

    virtual bool Initialize(ID3D11Device* Device) = 0;
    virtual void Release() = 0;

    uint32 GetAtlasSize() const { return AtlasSize; }
    uint32 GetSliceCount() const { return SliceCount; }

private:
    uint32 AtlasSize = 0;
    uint32 SliceCount = 0;
};
