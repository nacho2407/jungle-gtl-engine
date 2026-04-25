#include "ShadowAtlasManager.h"

void FShadowAtlasManager::Initialize(ID3D11Device* InDevice)
{
	if (InDevice == nullptr) return;

	ShadowMapAtlas.Initialize(InDevice);
}

bool FShadowAtlasManager::AllocateTile(FShadowAtlasTile& OutTile)
{
    const int32 TileSize = GetTileSize();

	int32 TileX, TileY;

    if (ShadowAllocator.AllocateTile(TileX, TileY))
    {
        OutTile.TileIndex = TileY * ShadowAllocator.TileCountX + TileX;

        OutTile.TileX = TileX;
        OutTile.TileY = TileY;

        OutTile.PixelX = TileX * TileSize;
        OutTile.PixelY = TileY * TileSize;

        OutTile.Width = TileSize;
        OutTile.Height = TileSize;

        const float AtlasW = static_cast<float>(GetAtlasWidth());
        const float AtlasH = static_cast<float>(GetAtlasHeight());
        const float Tile = static_cast<float>(TileSize);

        OutTile.ScaleOffset = FVector4(
            Tile / AtlasW,
            Tile / AtlasH,
            static_cast<float>(OutTile.PixelX) / AtlasW,
            static_cast<float>(OutTile.PixelY) / AtlasH);

        return true;
    }
	return false;
}

bool FShadowAtlasManager::FreeTile(const int32& TileIndex)
{
	if (TileIndex >= 0 && TileIndex < ShadowAllocator.TileCountX * ShadowAllocator.TileCountY)
	{
		int32 TileX = TileIndex % ShadowAllocator.TileCountX;
		int32 TileY = TileIndex / ShadowAllocator.TileCountX;
		ShadowAllocator.FreeTile(TileX, TileY);
		return true;
    }
    return false;
}
