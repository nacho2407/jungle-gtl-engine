#include <Windows.h>

extern "C"
{
	// NVIDIA Optimus: 고성능 NVIDIA GPU 사용 요청
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;

	// AMD PowerXpress: 고성능 AMD GPU 사용 요청
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}