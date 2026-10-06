#include "Cloth/ClothTypes.h"

const char* GetClothBackendName(EClothBackendType Backend)
{
	switch (Backend)
	{
	case EClothBackendType::CUDA:
		return "CUDA";
	case EClothBackendType::DX11:
		return "DX11";
	case EClothBackendType::CPU:
		return "CPU";
	case EClothBackendType::Disabled:
		return "Disabled";
	case EClothBackendType::Unavailable:
	default:
		return "Unavailable";
	}
}
