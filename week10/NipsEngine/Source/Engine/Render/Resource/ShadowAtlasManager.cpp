#include "ShadowAtlasManager.h"

void FShadowAtlasManager::Initialize(ID3D11Device* InDevice)
{
	if (InDevice == nullptr) return;

	ShadowMapAtlas.Initialize(InDevice);
}

bool FShadowAtlasManager::AllocateTile(int32& OutTileX, int32& OutTileY)
{
    for (int Y = 0; Y < ShadowMapAtlas.TileCountY; ++Y)
    {
        for (int X = 0; X < ShadowMapAtlas.TileCountX; ++X	)
        {
            int Index = Y * ShadowMapAtlas.TileCountX + X;
            if (!ShadowMapAtlas.TileUsed[Index])
            {
                ShadowMapAtlas.TileUsed[Index] = true;
                OutTileX = X;
                OutTileY = Y;
                return true;
            }
        }
    }
    return false;
}

bool FShadowAtlasManager::FreeTile(int32 X, int32 Y)
{
    int tileX = X / ShadowMapAtlas.TileSize;
    int tileY = Y / ShadowMapAtlas.TileSize;

    if (tileX < 0 || tileX >= ShadowMapAtlas.TileCountX || tileY < 0 || tileY >= ShadowMapAtlas.TileCountY)
	{
		return false;
	}
	int Index = tileY * ShadowMapAtlas.TileCountX + tileX;
	ShadowMapAtlas.TileUsed[Index] = false;
    return true;
}